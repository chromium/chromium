// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/fileapi_util.h"

#include <stddef.h>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "google_apis/drive/task_util.h"
#include "net/base/escape.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/open_file_system_mode.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
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
                                            const std::string& extension_id) {
  GURL base_url = storage::GetFileSystemRootURI(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id),
      storage::kFileSystemTypeExternal);
  return GURL(base_url.spec() +
              net::EscapeUrlEncodedData(relative_path.AsUTF8Unsafe(),
                                        false));  // Space to %20 instead of +.
}

// Creates an EntryDefinition object with an error set to |error|.
EntryDefinition CreateEntryDefinitionWithError(base::File::Error error) {
  EntryDefinition result;
  result.error = error;
  return result;
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
  if (!context.external_backend()->GetVirtualPath(file_path, &virtual_path))
    return false;

  const storage::FileSystemURL url = context.CreateCrackedFileSystemURL(
      url::Origin(), storage::kFileSystemTypeExternal, virtual_path);
  if (!url.is_valid())
    return false;

  return IsNonNativeFileSystemType(url.type());
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
      const GURL& origin,
      const SelectedFileInfoList& selected_info_list,
      FileChooserFileInfoListCallback callback)
      : context_(context),
        callback_(std::move(callback)) {
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
      if (!context->external_backend()->GetVirtualPath(
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

    // If the list includes at least one non-native file (without a snapshot
    // file), move to IO thread to obtain metadata for the non-native file.
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

  ~ConvertSelectedFileInfoListToFileChooserFileInfoListImpl() {
    for (const auto& info : chooser_info_list_) {
      if (info && info->is_file_system()) {
        storage::IsolatedContext::GetInstance()->RevokeFileSystem(
            context_->CrackURL(info->get_file_system()->url)
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
        context_->CrackURL((*it)->get_file_system()->url),
        storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
            storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
            storage::FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
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

  DISALLOW_COPY_AND_ASSIGN(
      ConvertSelectedFileInfoListToFileChooserFileInfoListImpl);
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
    int fields,
    storage::FileSystemOperationRunner::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  file_system_context->operation_runner()->GetMetadata(internal_url, fields,
                                                       std::move(callback));
}

}  // namespace

EntryDefinition::EntryDefinition() = default;

EntryDefinition::EntryDefinition(const EntryDefinition& other) = default;

EntryDefinition::~EntryDefinition() = default;

storage::FileSystemContext* GetFileSystemContextForExtensionId(
    Profile* profile,
    const std::string& extension_id) {
  return extensions::util::GetStoragePartitionForExtensionId(extension_id,
                                                             profile)
      ->GetFileSystemContext();
}

storage::FileSystemContext* GetFileSystemContextForRenderFrameHost(
    Profile* profile,
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetStoragePartition()->GetFileSystemContext();
}

bool ConvertAbsoluteFilePathToFileSystemUrl(Profile* profile,
                                            const base::FilePath& absolute_path,
                                            const std::string& extension_id,
                                            GURL* url) {
  base::FilePath relative_path;
  if (!ConvertAbsoluteFilePathToRelativeFileSystemPath(profile,
                                                       extension_id,
                                                       absolute_path,
                                                       &relative_path)) {
    return false;
  }
  *url = ConvertRelativeFilePathToFileSystemUrl(relative_path, extension_id);
  return true;
}

bool ConvertAbsoluteFilePathToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const base::FilePath& absolute_path,
    base::FilePath* virtual_path) {
  storage::ExternalFileSystemBackend* backend =
      GetFileSystemContextForExtensionId(profile, extension_id)
          ->external_backend();
  if (!backend)
    return false;

  // Find if this file path is managed by the external backend.
  if (!backend->GetVirtualPath(absolute_path, virtual_path))
    return false;

  return true;
}

// Helper class used to resolve a list of URLs. This class serves as a callback
// for ResolveURL method. set_barrier() must be called before any call to
// OnURLResolved() or AddEntry(). Each entry added calls the barrier, which
// determines if the overall collection is complete. The main reason for this
// class is so that it can hold on to |file_system_context| and |result|
// objects needed on OnURLResolved() callback.
class ResolvedURLCollector {
 public:
  ResolvedURLCollector(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      EntryDefinitionList* result)
      : file_system_context_(file_system_context), result_(result) {}

  // Sets the barrier that will be called on each added FileDefinition.
  void set_barrier(base::RepeatingClosure barrier) {
    barrier_ = std::move(barrier);
  }

  // Callback invoked by ResolveURL method of the FileSystemContext. The first
  // argument must be bound before this method is passed to ResolveURL.
  void OnURLResolved(const FileDefinition& fd,
                     base::File::Error error,
                     const storage::FileSystemInfo& info,
                     const base::FilePath& file_path,
                     storage::FileSystemContext::ResolvedEntryType type) {
    // Construct a target Entry.fullPath value from the virtual path and the
    // root URL. Eg. Downloads/A/b.txt -> A/b.txt.
    EntryDefinition entry_definition;
    storage::FileSystemURL fs_url =
        file_system_context_->CrackURL(info.root_url);
    if (error != base::File::FILE_OK) {
      entry_definition = CreateEntryDefinitionWithError(error);
    } else if (!fs_url.is_valid()) {
      entry_definition =
          CreateEntryDefinitionWithError(base::File::FILE_ERROR_NOT_FOUND);
    } else {
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
          entry_definition.is_directory = fd.is_directory;
          break;
      }
      entry_definition.error = base::File::FILE_OK;
      const base::FilePath root_virtual_path = fs_url.virtual_path();
      DCHECK(root_virtual_path == fd.virtual_path ||
             root_virtual_path.IsParent(fd.virtual_path));
      base::FilePath full_path;
      root_virtual_path.AppendRelativePath(fd.virtual_path, &full_path);
      entry_definition.full_path = full_path;
    }
    AddEntry(entry_definition);
  }

  void AddEntry(const EntryDefinition& entry) {
    DCHECK(!barrier_.is_null());
    result_->push_back(entry);
    barrier_.Run();
  }

 private:
  // Holds onto a reference counted pointer to the context associated with
  // the storage partition whose file descriptors are converted.
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  // The list filled with entries.
  EntryDefinitionList* result_;
  // Guards the done_callback_ call.
  base::RepeatingClosure barrier_;
};

void ConvertFileDefinitionListToEntryDefinitionList(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const url::Origin& origin,
    const FileDefinitionList& file_definition_list,
    EntryDefinitionListCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto result = std::make_unique<EntryDefinitionList>();

  if (!file_system_context.get()) {
    // Without file system context we cannot convert anything. We just
    // create entry definition with error for all files on the list.
    for (size_t i = file_definition_list.size(); i > 0; --i) {
      result->push_back(CreateEntryDefinitionWithError(
          base::File::FILE_ERROR_INVALID_OPERATION));
    }
    std::move(callback).Run(std::move(result));
    return;
  }

  // The barrier owns the collector so that it can delete it, once the final
  // callback is invoked. The collector must know about the barrier so it can
  // call it when adding entries. Hence the intricate, constructor, setter dance
  // we have here.
  auto collector = std::make_unique<ResolvedURLCollector>(
      file_system_context.get(), result.get());
  ResolvedURLCollector* collector_ptr = collector.get();
  collector_ptr->set_barrier(base::BarrierClosure(
      file_definition_list.size(),
      base::BindOnce(
          [](std::unique_ptr<ResolvedURLCollector> collector,
             std::unique_ptr<EntryDefinitionList> result,
             EntryDefinitionListCallback done_callback) {
            // collector will be deleted here, once this function is executed.
            std::move(done_callback).Run(std::move(result));
          },
          std::move(collector), std::move(result), std::move(callback))));

  for (const FileDefinition& fd : file_definition_list) {
    storage::FileSystemURL url =
        file_system_context->CreateCrackedFileSystemURL(
            origin, storage::kFileSystemTypeExternal, fd.virtual_path);
    if (url.is_valid()) {
      file_system_context->ResolveURL(
          url, base::BindOnce(&ResolvedURLCollector::OnURLResolved,
                              base::Unretained(collector_ptr), fd));
    } else {
      collector_ptr->AddEntry(
          CreateEntryDefinitionWithError(base::File::FILE_ERROR_NOT_FOUND));
    }
  }
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
    const GURL& origin,
    const SelectedFileInfoList& selected_info_list,
    FileChooserFileInfoListCallback callback) {
  // The object deletes itself.
  new ConvertSelectedFileInfoListToFileChooserFileInfoListImpl(
      context, origin, selected_info_list, std::move(callback));
}

std::unique_ptr<base::DictionaryValue> ConvertEntryDefinitionToValue(
    const EntryDefinition& entry_definition) {
  auto entry = std::make_unique<base::DictionaryValue>();
  entry->SetString("fileSystemName", entry_definition.file_system_name);
  entry->SetString("fileSystemRoot", entry_definition.file_system_root_url);
  entry->SetString(
      "fileFullPath",
      base::FilePath("/").Append(entry_definition.full_path).AsUTF8Unsafe());
  entry->SetBoolean("fileIsDirectory", entry_definition.is_directory);
  return entry;
}

std::unique_ptr<base::ListValue> ConvertEntryDefinitionListToListValue(
    const EntryDefinitionList& entry_definition_list) {
  auto entries = std::make_unique<base::ListValue>();
  for (auto it = entry_definition_list.begin();
       it != entry_definition_list.end(); ++it) {
    entries->Append(ConvertEntryDefinitionToValue(*it));
  }
  return entries;
}

void CheckIfDirectoryExists(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath& directory_path,
    storage::FileSystemOperationRunner::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
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
    int fields,
    storage::FileSystemOperationRunner::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
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
    const GURL& origin,
    const base::FilePath& virtual_path) {
  const storage::FileSystemURL original_url =
      context.CreateCrackedFileSystemURL(url::Origin::Create(origin),
                                         storage::kFileSystemTypeExternal,
                                         virtual_path);

  std::string register_name;
  storage::IsolatedContext::ScopedFSHandle file_system =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForPath(
          original_url.type(), original_url.filesystem_id(),
          original_url.path(), &register_name);
  storage::FileSystemURL isolated_url = context.CreateCrackedFileSystemURL(
      url::Origin::Create(origin), storage::kFileSystemTypeIsolated,
      base::FilePath(file_system.id()).Append(register_name));
  return {isolated_url, file_system};
}

}  // namespace util
}  // namespace file_manager
