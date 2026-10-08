// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"
#include "chrome/browser/media_galleries/fileapi/media_file_validator_factory.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"

using storage::FileSystemContext;
using storage::FileSystemURL;

namespace {

constexpr std::string_view kMediaGalleryMountPrefix = "media_galleries-";

base::LazyThreadPoolSequencedTaskRunner g_media_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN));

void OnPreferencesInit(
    const content::WebContents::Getter& web_contents_getter,
    const extensions::Extension* extension,
    MediaGalleryPrefId pref_id,
    base::OnceCallback<void(base::File::Error result)> callback) {
  content::WebContents* contents = web_contents_getter.Run();
  if (!contents) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::File::FILE_ERROR_FAILED));
    return;
  }
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  registry->RegisterMediaFileSystemForExtension(contents, extension, pref_id,
                                                std::move(callback));
}

void AttemptAutoMountOnUIThread(
    const content::WebContents::Getter& web_contents_getter,
    const std::string& storage_domain,
    const std::string& mount_point,
    base::OnceCallback<void(base::File::Error result)> callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  content::WebContents* web_contents = web_contents_getter.Run();
  if (web_contents) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());

    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(profile);
    const extensions::Extension* extension =
        extension_registry->enabled_extensions().GetByID(storage_domain);
    std::string expected_mount_prefix =
        MediaFileSystemBackend::ConstructMountName(
            profile->GetPath(), storage_domain, kInvalidMediaGalleryPrefId);
    MediaGalleryPrefId pref_id = kInvalidMediaGalleryPrefId;
    if (extension && extension->id() == storage_domain &&
        base::StartsWith(mount_point, expected_mount_prefix,
                         base::CompareCase::SENSITIVE) &&
        base::StringToUint64(mount_point.substr(expected_mount_prefix.size()),
                             &pref_id) &&
        pref_id != kInvalidMediaGalleryPrefId) {
      MediaGalleriesPreferences* preferences =
          g_browser_process->media_file_system_registry()->GetPreferences(
              profile);
      // Pass the WebContentsGetter to the closure to prevent a use-after-free
      // in the case that the web_contents is destroyed before the closure runs.
      preferences->EnsureInitialized(base::BindOnce(
          &OnPreferencesInit, web_contents_getter, base::RetainedRef(extension),
          pref_id, std::move(callback)));
      return;
    }
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::File::FILE_ERROR_NOT_FOUND));
}

content::WebContents* GetWebContentsFromFrameTreeNodeID(
    content::FrameTreeNodeId frame_tree_node_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  return content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
}

bool IsMediaGalleryAccessible(const storage::FileSystemURL& filesystem_url) {
  const std::string& filesystem_id = filesystem_url.filesystem_id();

  // If it's not a media gallery mount, this security check doesn't apply.
  // This allows unit tests and other internal components to bypass this check.
  if (!base::StartsWith(filesystem_id, kMediaGalleryMountPrefix,
                        base::CompareCase::SENSITIVE)) {
    return true;
  }

  // If it is a media gallery mount, it must be accessed by an extension.
  // Opaque origins (such as sandboxed iframes or data: URLs) and file:// URLs
  // naturally have empty hosts or non-extension schemes. We gracefully return
  // false here to deny access rather than treating it as a fatal IPC error, as
  // this can happen from benign web developer mistakes.
  if (filesystem_url.origin().scheme() != extensions::kExtensionScheme ||
      filesystem_url.origin().host().empty()) {
    return false;
  }

  std::optional<MediaFileSystemBackend::ParsedMountName> parsed =
      MediaFileSystemBackend::ParseMountName(filesystem_id);
  return parsed.has_value() &&
         parsed->extension_id == filesystem_url.origin().host();
}

}  // namespace

MediaFileSystemBackend::MediaFileSystemBackend(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      media_copy_or_move_file_validator_factory_(
          std::make_unique<MediaFileValidatorFactory>()),
      native_media_file_util_(
          std::make_unique<NativeMediaFileUtil>(g_media_task_runner.Get())),
      device_media_async_file_util_(
          DeviceMediaAsyncFileUtil::Create(profile_path_,
                                           APPLY_MEDIA_FILE_VALIDATION)) {}

MediaFileSystemBackend::~MediaFileSystemBackend() = default;

// static
void MediaFileSystemBackend::AssertCurrentlyOnMediaSequence() {
#if DCHECK_IS_ON()
  CHECK(g_media_task_runner.Get()->RunsTasksInCurrentSequence(),
        base::NotFatalUntil::M161);
#endif
}

// static
scoped_refptr<base::SequencedTaskRunner>
MediaFileSystemBackend::MediaTaskRunner() {
  return g_media_task_runner.Get();
}

// static
std::string MediaFileSystemBackend::ConstructMountName(
    const base::FilePath& profile_path,
    const std::string& extension_id,
    MediaGalleryPrefId pref_id) {
  std::string name(kMediaGalleryMountPrefix);
  name.append(profile_path.BaseName().MaybeAsASCII());
  name.append("-");
  name.append(extension_id);
  name.append("-");
  if (pref_id != kInvalidMediaGalleryPrefId)
    name.append(base::NumberToString(pref_id));
  base::ReplaceChars(name, " /", "_", &name);
  return name;
}

// static
std::optional<MediaFileSystemBackend::ParsedMountName>
MediaFileSystemBackend::ParseMountName(const std::string& mount_name) {
  if (!base::StartsWith(mount_name, kMediaGalleryMountPrefix,
                        base::CompareCase::SENSITIVE)) {
    return std::nullopt;
  }

  // Strip the "media_galleries-" prefix. The remainder has the format:
  // "<profile_base_name>-<extension_id>-<pref_id>". Because `profile_base_name`
  // may itself contain hyphens, parse from the right.
  std::string_view remainder =
      std::string_view(mount_name).substr(kMediaGalleryMountPrefix.size());

  size_t pref_separator = remainder.rfind('-');
  if (pref_separator == std::string_view::npos) {
    return std::nullopt;
  }

  std::string_view pref_id_str = remainder.substr(pref_separator + 1);
  MediaGalleryPrefId parsed_pref_id = kInvalidMediaGalleryPrefId;
  if (!base::StringToUint64(pref_id_str, &parsed_pref_id) ||
      parsed_pref_id == kInvalidMediaGalleryPrefId) {
    return std::nullopt;
  }

  std::string_view prefix_and_ext = remainder.substr(0, pref_separator);
  size_t ext_separator = prefix_and_ext.rfind('-');
  if (ext_separator == std::string_view::npos) {
    return std::nullopt;
  }

  std::string_view parsed_profile = prefix_and_ext.substr(0, ext_separator);
  std::string_view parsed_extension_id =
      prefix_and_ext.substr(ext_separator + 1);
  if (parsed_profile.empty() || parsed_extension_id.empty()) {
    return std::nullopt;
  }

  return ParsedMountName{
      .profile_base_name = std::string(parsed_profile),
      .extension_id = std::string(parsed_extension_id),
      .pref_id = parsed_pref_id,
  };
}

// static
bool MediaFileSystemBackend::AttemptAutoMountForURLRequest(
    const storage::FileSystemRequestInfo& request_info,
    const storage::FileSystemURL& filesystem_url,
    base::OnceCallback<void(base::File::Error result)> callback) {
  if (request_info.storage_domain.empty() ||
      filesystem_url.type() != storage::kFileSystemTypeExternal ||
      request_info.storage_domain != filesystem_url.origin().host()) {
    return false;
  }

  const base::FilePath& virtual_path = filesystem_url.path();
  if (virtual_path.ReferencesParent())
    return false;
  std::vector<base::FilePath::StringType> components =
      virtual_path.GetComponents();
  if (components.empty())
    return false;
  std::string mount_point = base::FilePath(components[0]).AsUTF8Unsafe();
  if (!base::StartsWith(mount_point, kMediaGalleryMountPrefix,
                        base::CompareCase::SENSITIVE))
    return false;

  content::WebContents::Getter web_contents_getter =
      base::BindRepeating(&GetWebContentsFromFrameTreeNodeID,
                          content::FrameTreeNodeId(request_info.content_id));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AttemptAutoMountOnUIThread, web_contents_getter,
                     request_info.storage_domain, mount_point,
                     std::move(callback)));
  return true;
}

bool MediaFileSystemBackend::CanHandleType(storage::FileSystemType type) const {
  switch (type) {
    case storage::kFileSystemTypeLocalMedia:
    case storage::kFileSystemTypeDeviceMedia:
      return true;
    default:
      return false;
  }
}

void MediaFileSystemBackend::Initialize(storage::FileSystemContext* context) {
}

void MediaFileSystemBackend::ResolveURL(const FileSystemURL& url,
                                        storage::OpenFileSystemMode mode,
                                        ResolveURLCallback callback) {
  // We never allow opening a new FileSystem via usual ResolveURL.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GURL(), std::string(),
                                base::File::FILE_ERROR_SECURITY));
}

storage::AsyncFileUtil* MediaFileSystemBackend::GetAsyncFileUtil(
    storage::FileSystemType type) {
  // We count file system usages here, because we want to count (per session)
  // when the file system is actually used for I/O, rather than merely present.
  switch (type) {
    case storage::kFileSystemTypeLocalMedia:
      return native_media_file_util_.get();
    case storage::kFileSystemTypeDeviceMedia:
      return device_media_async_file_util_.get();
    default:
      NOTREACHED();
  }
}

storage::WatcherManager* MediaFileSystemBackend::GetWatcherManager(
    storage::FileSystemType type) {
  return nullptr;
}

storage::CopyOrMoveFileValidatorFactory*
MediaFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    storage::FileSystemType type,
    base::File::Error* error_code) {
  CHECK(error_code, base::NotFatalUntil::M161);
  *error_code = base::File::FILE_OK;
  switch (type) {
    case storage::kFileSystemTypeLocalMedia:
    case storage::kFileSystemTypeDeviceMedia:
      if (!media_copy_or_move_file_validator_factory_) {
        *error_code = base::File::FILE_ERROR_SECURITY;
        return nullptr;
      }
      return media_copy_or_move_file_validator_factory_.get();
    default:
      NOTREACHED();
  }
}

std::unique_ptr<storage::FileSystemOperation>
MediaFileSystemBackend::CreateFileSystemOperation(
    storage::OperationType type,
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  if (!IsMediaGalleryAccessible(url)) {
    *error_code = base::File::FILE_ERROR_SECURITY;
    return nullptr;
  }

  std::unique_ptr<storage::FileSystemOperationContext> operation_context(
      std::make_unique<storage::FileSystemOperationContext>(
          context, MediaTaskRunner().get()));
  return storage::FileSystemOperation::Create(type, url, context,
                                              std::move(operation_context));
}

bool MediaFileSystemBackend::SupportsStreaming(
    const storage::FileSystemURL& url) const {
  if (url.type() == storage::kFileSystemTypeDeviceMedia)
    return device_media_async_file_util_->SupportsStreaming(url);

  return false;
}

bool MediaFileSystemBackend::HasInplaceCopyImplementation(
    storage::FileSystemType type) const {
  CHECK(type == storage::kFileSystemTypeLocalMedia ||
            type == storage::kFileSystemTypeDeviceMedia,
        base::NotFatalUntil::M161);
  return true;
}

std::unique_ptr<storage::FileStreamReader>
MediaFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    FileSystemContext* context,
    file_access::ScopedFileAccessDelegate::
        RequestFilesAccessIOCallback /*file_access*/) const {
  if (!IsMediaGalleryAccessible(url)) {
    return nullptr;
  }

  if (url.type() == storage::kFileSystemTypeDeviceMedia) {
    std::unique_ptr<storage::FileStreamReader> reader =
        device_media_async_file_util_->GetFileStreamReader(
            url, offset, expected_modification_time, context);
    CHECK(reader, base::NotFatalUntil::M161);
    return reader;
  }

  return storage::FileStreamReader::CreateForLocalFile(
      context->default_file_task_runner(), url.path(), offset,
      expected_modification_time);
}

std::unique_ptr<storage::FileStreamWriter>
MediaFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset,
    FileSystemContext* context) const {
  if (!IsMediaGalleryAccessible(url)) {
    return nullptr;
  }

  return storage::FileStreamWriter::CreateForLocalFile(
      context->default_file_task_runner(), url.path(), offset,
      storage::FileStreamWriter::OPEN_EXISTING_FILE);
}

storage::FileSystemQuotaUtil* MediaFileSystemBackend::GetQuotaUtil() {
  // No quota support.
  return nullptr;
}

const storage::UpdateObserverList* MediaFileSystemBackend::GetUpdateObservers(
    storage::FileSystemType type) const {
  return nullptr;
}

const storage::ChangeObserverList* MediaFileSystemBackend::GetChangeObservers(
    storage::FileSystemType type) const {
  return nullptr;
}

const storage::AccessObserverList* MediaFileSystemBackend::GetAccessObservers(
    storage::FileSystemType type) const {
  return nullptr;
}
