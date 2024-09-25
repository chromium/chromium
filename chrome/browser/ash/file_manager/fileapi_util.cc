// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/fileapi_util.h"

#include <stddef.h>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/extension_util.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/open_file_system_mode.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserThread;

namespace file_manager {
namespace util {

using blink::mojom::FileChooserFileInfo;
using blink::mojom::FileSystemFileInfo;
using blink::mojom::NativeFileInfo;

namespace {

GURL ConvertRelativeFilePathToFileSystemUrl(const base::FilePath& relative_path,
                                            const GURL& source_url) {
  GURL base_url = storage::GetFileSystemRootURI(
      source_url, storage::kFileSystemTypeExternal);
  return GURL(base_url.spec() +
              base::EscapeUrlEncodedData(relative_path.AsUTF8Unsafe(),
                                         false));  // Space to %20 instead of +.
}

// Creates an ErrorDefinition with an error set to |error|.
EntryDefinition CreateEntryDefinitionWithError(base::File::Error error) {
  EntryDefinition result;
  result.error = error;
  return result;
}

// Helper class for performing conversions from file definitions to entry
// definitions. It is possible to do it without a class, but the code would be
// crazy and super tricky.
//
// This class copies the input |file_definition_list|,
// so there is no need to worry about validity of passed |file_definition_list|
// reference. Also, it automatically deletes itself after converting finished,
// or if shutdown is invoked during ResolveURL(). Must be called on UI thread.
class FileDefinitionListConverter {
 public:
  FileDefinitionListConverter(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      const url::Origin& origin,
      const FileDefinitionList& file_definition_list,
      EntryDefinitionListCallback callback);
  ~FileDefinitionListConverter() = default;

 private:
  // Converts the element under the iterator to an entry. First, converts
  // the virtual path to an URL, and calls OnResolvedURL(). In case of error
  // calls OnIteratorConverted with an error entry definition.
  void ConvertNextIterator(
      std::unique_ptr<FileDefinitionListConverter> self_deleter,
      FileDefinitionList::const_iterator iterator);

  // Creates an entry definition from the URL as well as the file definition.
  // Then, calls OnIteratorConverted with the created entry definition.
  void OnResolvedURL(std::unique_ptr<FileDefinitionListConverter> self_deleter,
                     FileDefinitionList::const_iterator iterator,
                     base::File::Error error,
                     const storage::FileSystemInfo& info,
                     const base::FilePath& file_path,
                     storage::FileSystemContext::ResolvedEntryType type);

  // Called when the iterator is converted. Adds the |entry_definition| to
  // |results_| and calls ConvertNextIterator() for the next element.
  void OnIteratorConverted(
      std::unique_ptr<FileDefinitionListConverter> self_deleter,
      FileDefinitionList::const_iterator iterator,
      const EntryDefinition& entry_definition);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const url::Origin origin_;
  const FileDefinitionList file_definition_list_;
  EntryDefinitionListCallback callback_;
  std::unique_ptr<EntryDefinitionList> result_;
};

FileDefinitionListConverter::FileDefinitionListConverter(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const url::Origin& origin,
    const FileDefinitionList& file_definition_list,
    EntryDefinitionListCallback callback)
    : file_system_context_(file_system_context),
      origin_(origin),
      file_definition_list_(file_definition_list),
      callback_(std::move(callback)),
      result_(new EntryDefinitionList) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Deletes the converter, once the scoped pointer gets out of scope. It is
  // either, if the conversion is finished, or ResolveURL() is terminated, and
  // the callback not called because of shutdown.
  std::unique_ptr<FileDefinitionListConverter> self_deleter(this);
  ConvertNextIterator(std::move(self_deleter), file_definition_list_.begin());
}

void FileDefinitionListConverter::ConvertNextIterator(
    std::unique_ptr<FileDefinitionListConverter> self_deleter,
    FileDefinitionList::const_iterator iterator) {
  if (iterator == file_definition_list_.end()) {
    // The converter object will be destroyed since |self_deleter| gets out of
    // scope.
    std::move(callback_).Run(std::move(result_));
    return;
  }

  if (!file_system_context_.get()) {
    OnIteratorConverted(std::move(self_deleter), iterator,
                        CreateEntryDefinitionWithError(
                            base::File::FILE_ERROR_INVALID_OPERATION));
    return;
  }

  storage::FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFirstParty(origin_),
      storage::kFileSystemTypeExternal, iterator->virtual_path);

  if (!url.is_valid()) {
    OnIteratorConverted(
        std::move(self_deleter), iterator,
        CreateEntryDefinitionWithError(base::File::FILE_ERROR_NOT_FOUND));
    return;
  }

  // The converter object will be deleted if the callback is not called because
  // of shutdown during ResolveURL().
  file_system_context_->ResolveURL(
      url, base::BindOnce(&FileDefinitionListConverter::OnResolvedURL,
                          base::Unretained(this), std::move(self_deleter),
                          iterator));
}

void FileDefinitionListConverter::OnResolvedURL(
    std::unique_ptr<FileDefinitionListConverter> self_deleter,
    FileDefinitionList::const_iterator iterator,
    base::File::Error error,
    const storage::FileSystemInfo& info,
    const base::FilePath& file_path,
    storage::FileSystemContext::ResolvedEntryType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error != base::File::FILE_OK) {
    OnIteratorConverted(std::move(self_deleter), iterator,
                        CreateEntryDefinitionWithError(error));
    return;
  }

  EntryDefinition entry_definition;
  entry_definition.file_system_root_url = info.root_url.spec();
  entry_definition.file_system_name = info.name;
  switch (type) {
    case storage::FileSystemContext::RESOLVED_ENTRY_FILE:
      entry_definition.is_directory = false;
      break;
    case storage::FileSystemContext::RESOLVED_ENTRY_DIRECTORY:
      entry_definition.is_directory = true;
      break;
    case storage::FileSystemContext::RESOLVED_ENTRY_NOT_FOUND:
      entry_definition.is_directory = iterator->is_directory;
      break;
  }
  entry_definition.error = base::File::FILE_OK;

  // Construct a target Entry.fullPath value from the virtual path and the
  // root URL. Eg. Downloads/A/b.txt -> A/b.txt.
  storage::FileSystemURL fs_url =
      file_system_context_->CrackURLInFirstPartyContext(info.root_url);
  if (!fs_url.is_valid()) {
    OnIteratorConverted(
        std::move(self_deleter), iterator,
        CreateEntryDefinitionWithError(base::File::FILE_ERROR_NOT_FOUND));
    return;
  }
  const base::FilePath root_virtual_path = fs_url.virtual_path();
  DCHECK(root_virtual_path == iterator->virtual_path ||
         root_virtual_path.IsParent(iterator->virtual_path));
  base::FilePath full_path;
  root_virtual_path.AppendRelativePath(iterator->virtual_path, &full_path);
  entry_definition.full_path = full_path;

  OnIteratorConverted(std::move(self_deleter), iterator, entry_definition);
}

void FileDefinitionListConverter::OnIteratorConverted(
    std::unique_ptr<FileDefinitionListConverter> self_deleter,
    FileDefinitionList::const_iterator iterator,
    const EntryDefinition& entry_definition) {
  result_->push_back(entry_definition);
  ConvertNextIterator(std::move(self_deleter), ++iterator);
}

// Helper function to return the converted definition entry directly, without
// the redundant container.
void OnConvertFileDefinitionDone(
    EntryDefinitionCallback callback,
    std::unique_ptr<EntryDefinitionList> entry_definition_list) {
  DCHECK_EQ(1u, entry_definition_list->size());
  std::move(callback).Run(entry_definition_list->at(0));
}

// Checks if the |file_path| points non-native location or not.
bool IsUnderNonNativeLocalPath(const storage::FileSystemContext& context,
                               const base::FilePath& file_path) {
  base::FilePath virtual_path;
  if (!ash::FileSystemBackend::Get(context)->GetVirtualPath(file_path,
                                                            &virtual_path)) {
    return false;
  }

  const storage::FileSystemURL url = context.CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeExternal, virtual_path);
  if (!url.is_valid()) {
    return false;
  }

  return !url.TypeImpliesPathIsReal();
}

// Helper class to convert SelectedFileInfoList into ChooserFileInfoList.
class ConvertSelectedFileInfoListToFileChooserFileInfoListImpl {
 public:
  // The scoped pointer to control lifetime of the instance itself. The pointer
  // is passed to callback functions and binds the lifetime of the instance to
  // the callback's lifetime.
  typedef std::unique_ptr<
      ConvertSelectedFileInfoListToFileChooserFileInfoListImpl>
      Lifetime;

  ConvertSelectedFileInfoListToFileChooserFileInfoListImpl(
      storage::FileSystemContext* context,
      const url::Origin& origin,
      const SelectedFileInfoList& selected_info_list,
      FileChooserFileInfoListCallback callback)
      : context_(context), callback_(std::move(callback)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    Lifetime lifetime(this);
    bool need_fill_metadata = false;

    for (size_t i = 0; i < selected_info_list.size(); ++i) {
      // Native file.
      if (!IsUnderNonNativeLocalPath(*context,
                                     selected_info_list[i].file_path)) {
        chooser_info_list_.push_back(
            FileChooserFileInfo::NewNativeFile(NativeFileInfo::New(
                selected_info_list[i].file_path,
                base::UTF8ToUTF16(selected_info_list[i].display_name))));
        continue;
      }

      // Non-native file, but it has a native snapshot file.
      if (!selected_info_list[i].local_path.empty()) {
        chooser_info_list_.push_back(
            FileChooserFileInfo::NewNativeFile(NativeFileInfo::New(
                selected_info_list[i].local_path,
                base::UTF8ToUTF16(selected_info_list[i].display_name))));
        continue;
      }

      // Non-native file without a snapshot file.
      base::FilePath virtual_path;
      if (!ash::FileSystemBackend::Get(*context)->GetVirtualPath(
              selected_info_list[i].file_path, &virtual_path)) {
        NotifyError(std::move(lifetime));
        return;
      }

      FileSystemURLAndHandle isolated_file_system_url_and_handle =
          CreateIsolatedURLFromVirtualPath(*context_, origin, virtual_path);
      const GURL url = isolated_file_system_url_and_handle.url.ToGURL();
      if (!url.is_valid()) {
        NotifyError(std::move(lifetime));
        return;
      }

      // Increase ref count of file system to keep it alive after |file_system|
      // goes out of scope. Our destructor will eventually revoke the file
      // system.
      storage::IsolatedContext::GetInstance()->AddReference(
          isolated_file_system_url_and_handle.handle.id());

      auto fs_info = FileSystemFileInfo::New();
      fs_info->url = url;
      chooser_info_list_.push_back(
          FileChooserFileInfo::NewFileSystem(std::move(fs_info)));
      need_fill_metadata = true;
    }

    // If the list includes at least one non-native file (wihtout a snapshot
    // file), move to IO thread to obtian metadata for the non-native file.
    if (need_fill_metadata) {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ConvertSelectedFileInfoListToFileChooserFileInfoListImpl::
                  FillMetadataOnIOThread,
              base::Unretained(this), std::move(lifetime),
              chooser_info_list_.begin()));
      return;
    }

    NotifyComplete(std::move(lifetime));
  }

  ConvertSelectedFileInfoListToFileChooserFileInfoListImpl(
      const ConvertSelectedFileInfoListToFileChooserFileInfoListImpl&) = delete;
  ConvertSelectedFileInfoListToFileChooserFileInfoListImpl& operator=(
      const ConvertSelectedFileInfoListToFileChooserFileInfoListImpl&) = delete;

  ~ConvertSelectedFileInfoListToFileChooserFileInfoListImpl() {
    for (const auto& info : chooser_info_list_) {
      if (info && info->is_file_system()) {
        storage::IsolatedContext::GetInstance()->RevokeFileSystem(
            context_->CrackURLInFirstPartyContext(info->get_file_system()->url)
                .mount_filesystem_id());
      }
    }
  }

 private:
  // Obtains metadata for the non-native file |it|.
  void FillMetadataOnIOThread(Lifetime lifetime,
                              const FileChooserFileInfoList::iterator& it) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    if (it == chooser_info_list_.end()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ConvertSelectedFileInfoListToFileChooserFileInfoListImpl::
                  NotifyComplete,
              base::Unretained(this), std::move(lifetime)));
      return;
    }

    if ((*it)->is_native_file()) {
      FillMetadataOnIOThread(std::move(lifetime), it + 1);
      return;
    }

    context_->operation_runner()->GetMetadata(
        context_->CrackURLInFirstPartyContext((*it)->get_file_system()->url),
        {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
         storage::FileSystemOperation::GetMetadataField::kSize,
         storage::FileSystemOperation::GetMetadataField::kLastModified},
        base::BindOnce(
            &ConvertSelectedFileInfoListToFileChooserFileInfoListImpl::
                OnGotMetadataOnIOThread,
            base::Unretained(this), std::move(lifetime), it));
  }

  // Callback invoked after GetMetadata.
  void OnGotMetadataOnIOThread(Lifetime lifetime,
                               const FileChooserFileInfoList::iterator& it,
                               base::File::Error result,
                               const base::File::Info& file_info) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    if (result != base::File::FILE_OK) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ConvertSelectedFileInfoListToFileChooserFileInfoListImpl::
                  NotifyError,
              base::Unretained(this), std::move(lifetime)));
      return;
    }

    (*it)->get_file_system()->length = file_info.size;
    (*it)->get_file_system()->modification_time = file_info.last_modified;
    DCHECK(!file_info.is_directory);
    FillMetadataOnIOThread(std::move(lifetime), it + 1);
  }

  // Returns a result to the |callback_|.
  void NotifyComplete(Lifetime /* lifetime */) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // Move the list content so that the file systems are not revoked at the
    // destructor.
    std::move(callback_).Run(std::move(chooser_info_list_));
  }

  // Returns an empty list to the |callback_|.
  void NotifyError(Lifetime /* lifetime */) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::move(callback_).Run(FileChooserFileInfoList());
  }

  scoped_refptr<storage::FileSystemContext> context_;
  FileChooserFileInfoList chooser_info_list_;
  FileChooserFileInfoListCallback callback_;
};

void CheckIfDirectoryExistsOnIoThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& internal_url,
    storage::FileSystemOperationRunner::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->DirectoryExists(internal_url,
                                                           std::move(callback));
}

void GetMetadataForPathOnIoThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& internal_url,
    storage::FileSystemOperationRunner::GetMetadataFieldSet fields,
    storage::FileSystemOperationRunner::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->GetMetadata(internal_url, fields,
                                                       std::move(callback));
}

// Helper struct used to store the current file url being checked when trying to
// find an unused filename.
struct GenerateUnusedFilenameState {
  scoped_refptr<storage::FileSystemContext> file_system_context;
  storage::FileSystemURL destination_folder;

  // The filename without the counter and extension.
  std::string prefix;

  // The number to check.
  int counter = 0;

  // The extension of the given filename, including the "." at the start.
  std::string extension;
};

// Helper callback function for GetUnusedFilename().
void GenerateUnusedFilenameOnGotMetadata(
    storage::FileSystemURL trial_url,
    GenerateUnusedFilenameState state,
    base::OnceCallback<void(base::FileErrorOr<storage::FileSystemURL>)>
        callback,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (error == base::File::FILE_ERROR_NOT_FOUND) {
    std::move(callback).Run(std::move(trial_url));
    return;
  } else if (error != base::File::FILE_OK &&
             error != base::File::FILE_ERROR_NOT_A_DIRECTORY) {
    std::move(callback).Run(base::unexpected(error));
    return;
  }

  // File at |trial_url| exists, so try the next number up.
  auto file_system_context = state.file_system_context;
  state.counter++;
  std::string filename =
      base::StringPrintf("%s (%d)%s", state.prefix.c_str(), state.counter,
                         state.extension.c_str());
  auto filesystem_url = file_system_context->CreateCrackedFileSystemURL(
      state.destination_folder.storage_key(),
      state.destination_folder.mount_type(),
      state.destination_folder.virtual_path().Append(
          base::FilePath::FromUTF8Unsafe(filename)));
  CheckIfDirectoryExistsOnIoThread(
      file_system_context, filesystem_url,
      base::BindOnce(&GenerateUnusedFilenameOnGotMetadata, filesystem_url,
                     std::move(state), std::move(callback)));
}

// If the file is on ODFS (OneDrive), trim leading and trailing spaces from the
// destination name because OneDrive will not allow this, even though Files app
// is fine with it.
base::FilePath TrimFilenameIfOnODFS(storage::FileSystemURL destination_folder,
                                    base::FilePath filename) {
  if (ash::cloud_upload::UrlIsOnODFS(destination_folder)) {
    std::string name = filename.AsUTF8Unsafe();
    base::TrimString(name, " ", &name);
    return base::FilePath(name);
  }

  return filename;
}

}  // namespace

EntryDefinition::EntryDefinition() = default;

EntryDefinition::EntryDefinition(const EntryDefinition& other) = default;

EntryDefinition::~EntryDefinition() = default;

const GURL GetFileManagerURL() {
  return GURL(ash::file_manager::kChromeUIFileManagerURL);
}

bool IsFileManagerURL(const GURL& source_url) {
  return GetFileManagerURL() == source_url.DeprecatedGetOriginAsURL();
}

storage::FileSystemContext* GetFileManagerFileSystemContext(
    content::BrowserContext* browser_context) {
  return GetFileSystemContextForSourceURL(browser_context, GetFileManagerURL());
}

storage::FileSystemContext* GetFileSystemContextForSourceURL(
    content::BrowserContext* browser_context,
    const GURL& source_url) {
  content::StoragePartition* const partition =
      content::HasWebUIScheme(source_url)
          ? browser_context->GetDefaultStoragePartition()
          : extensions::util::GetStoragePartitionForExtensionId(
                source_url.host(), browser_context);
  return partition->GetFileSystemContext();
}

storage::FileSystemContext* GetFileSystemContextForRenderFrameHost(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetStoragePartition()->GetFileSystemContext();
}

bool ConvertAbsoluteFilePathToFileSystemUrl(
    content::BrowserContext* browser_context,
    const base::FilePath& absolute_path,
    const GURL& source_url,
    GURL* url) {
  base::FilePath relative_path;
  if (!ConvertAbsoluteFilePathToRelativeFileSystemPath(
          browser_context, source_url, absolute_path, &relative_path)) {
    return false;
  }
  *url = ConvertRelativeFilePathToFileSystemUrl(relative_path, source_url);
  return true;
}

bool ConvertAbsoluteFilePathToRelativeFileSystemPath(
    content::BrowserContext* browser_context,
    const GURL& source_url,
    const base::FilePath& absolute_path,
    base::FilePath* virtual_path) {
  auto* backend = ash::FileSystemBackend::Get(
      *GetFileSystemContextForSourceURL(browser_context, source_url));
  if (!backend) {
    return false;
  }

  // Find if this file path is managed by the external backend.
  if (!backend->GetVirtualPath(absolute_path, virtual_path)) {
    return false;
  }

  return true;
}

void ConvertFileDefinitionListToEntryDefinitionList(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const url::Origin& origin,
    const FileDefinitionList& file_definition_list,
    EntryDefinitionListCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The converter object destroys itself.
  new FileDefinitionListConverter(file_system_context, origin,
                                  file_definition_list, std::move(callback));
}

void ConvertFileDefinitionToEntryDefinition(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const url::Origin& origin,
    const FileDefinition& file_definition,
    EntryDefinitionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  FileDefinitionList file_definition_list;
  file_definition_list.push_back(file_definition);
  ConvertFileDefinitionListToEntryDefinitionList(
      file_system_context, origin, file_definition_list,
      base::BindOnce(&OnConvertFileDefinitionDone, std::move(callback)));
}

void ConvertSelectedFileInfoListToFileChooserFileInfoList(
    storage::FileSystemContext* context,
    const url::Origin& origin,
    const SelectedFileInfoList& selected_info_list,
    FileChooserFileInfoListCallback callback) {
  // The object deletes itself.
  new ConvertSelectedFileInfoListToFileChooserFileInfoListImpl(
      context, origin, selected_info_list, std::move(callback));
}

base::Value::Dict ConvertEntryDefinitionToValue(
    const EntryDefinition& entry_definition) {
  base::Value::Dict entry;
  entry.Set("fileSystemName", entry_definition.file_system_name);
  entry.Set("fileSystemRoot", entry_definition.file_system_root_url);
  entry.Set(
      "fileFullPath",
      base::FilePath("/").Append(entry_definition.full_path).AsUTF8Unsafe());
  entry.Set("fileIsDirectory", entry_definition.is_directory);
  return entry;
}

base::Value::List ConvertEntryDefinitionListToListValue(
    const EntryDefinitionList& entry_definition_list) {
  base::Value::List entries;
  for (const auto& entry : entry_definition_list) {
    entries.Append(ConvertEntryDefinitionToValue(entry));
  }
  return entries;
}

void CheckIfDirectoryExists(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath& directory_path,
    storage::FileSystemOperationRunner::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* const backend = ash::FileSystemBackend::Get(*file_system_context);
  DCHECK(backend);
  const storage::FileSystemURL internal_url =
      backend->CreateInternalURL(file_system_context.get(), directory_path);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CheckIfDirectoryExistsOnIoThread, file_system_context,
                     internal_url,
                     google_apis::CreateRelayCallback(std::move(callback))));
}

void GetMetadataForPath(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath& entry_path,
    storage::FileSystemOperationRunner::GetMetadataFieldSet fields,
    storage::FileSystemOperationRunner::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* const backend = ash::FileSystemBackend::Get(*file_system_context);
  DCHECK(backend);
  const storage::FileSystemURL internal_url =
      backend->CreateInternalURL(file_system_context.get(), entry_path);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetMetadataForPathOnIoThread, file_system_context,
                     internal_url, fields,
                     google_apis::CreateRelayCallback(std::move(callback))));
}

FileSystemURLAndHandle CreateIsolatedURLFromVirtualPath(
    const storage::FileSystemContext& context,
    const url::Origin& origin,
    const base::FilePath& virtual_path) {
  const storage::FileSystemURL original_url =
      context.CreateCrackedFileSystemURL(
          blink::StorageKey::CreateFirstParty(origin),
          storage::kFileSystemTypeExternal, virtual_path);

  std::string register_name;
  storage::IsolatedContext::ScopedFSHandle file_system =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForPath(
          original_url.type(), original_url.filesystem_id(),
          original_url.path(), &register_name);
  storage::FileSystemURL isolated_url = context.CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFirstParty(origin),
      storage::kFileSystemTypeIsolated,
      base::FilePath(file_system.id()).Append(register_name));
  return {isolated_url, file_system};
}

void GenerateUnusedFilename(
    storage::FileSystemURL destination_folder,
    base::FilePath filename,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::OnceCallback<void(base::FileErrorOr<storage::FileSystemURL>)>
        callback) {
  if (filename.empty() || filename != filename.BaseName()) {
    std::move(callback).Run(
        base::unexpected(base::File::FILE_ERROR_INVALID_OPERATION));
    return;
  }

  base::FilePath trimmed_filename =
      TrimFilenameIfOnODFS(destination_folder, filename);

  auto trial_url = file_system_context->CreateCrackedFileSystemURL(
      destination_folder.storage_key(), destination_folder.mount_type(),
      destination_folder.virtual_path().Append(trimmed_filename));

  GenerateUnusedFilenameState state;
  state.destination_folder = std::move(destination_folder);
  state.file_system_context = file_system_context;
  state.extension = trimmed_filename.Extension();
  // Extracts the filename without extension or existing counter.
  // E.g. "foo (3).txt" -> "foo".
  RE2::Options options;
  options.set_dot_nl(true);  // Dot matches a new line.
  const RE2 re(R"((.*?)(?: \(\d+\))?)", options);
  const bool res = RE2::FullMatch(trimmed_filename.RemoveExtension().value(),
                                  re, &state.prefix);
  DCHECK(res) << " for '" << trimmed_filename << "'";

  auto get_metadata_callback = base::BindOnce(
      &GenerateUnusedFilenameOnGotMetadata, trial_url, std::move(state),
      google_apis::CreateRelayCallback(std::move(callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CheckIfDirectoryExistsOnIoThread, file_system_context,
                     std::move(trial_url), std::move(get_metadata_callback)));
}

}  // namespace util
}  // namespace file_manager
