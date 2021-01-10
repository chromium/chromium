// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/util/values/values_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/native_file_system/native_file_system_permission_context_factory.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/ui/native_file_system_dialogs.h"
#include "chrome/common/chrome_paths.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace {

using HandleType = content::NativeFileSystemPermissionContext::HandleType;

// Dictionary keys for the FILE_SYSTEM_LAST_PICKED_DIRECTORY website setting.
const char kLastPickedDirectoryKey[] = "default-path";
const char kLastPickedDirectoryTypeKey[] = "default-path-type";

void ShowNativeFileSystemRestrictedDirectoryDialogOnUIThread(
    content::GlobalFrameRoutingId frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    base::OnceCallback<
        void(ChromeNativeFileSystemPermissionContext::SensitiveDirectoryResult)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
  if (!rfh || !rfh->IsCurrent()) {
    // Requested from a no longer valid render frame host.
    std::move(callback).Run(ChromeNativeFileSystemPermissionContext::
                                SensitiveDirectoryResult::kAbort);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(ChromeNativeFileSystemPermissionContext::
                                SensitiveDirectoryResult::kAbort);
    return;
  }

  ShowNativeFileSystemRestrictedDirectoryDialog(
      origin, path, handle_type, std::move(callback), web_contents);
}

// Sentinel used to indicate that no PathService key is specified for a path in
// the struct below.
constexpr const int kNoBasePathKey = -1;

enum BlockType {
  kBlockAllChildren,
  kBlockNestedDirectories,
  kDontBlockChildren
};

const struct {
  // base::BasePathKey value (or one of the platform specific extensions to it)
  // for a path that should be blocked. Specify kNoBasePathKey if |path| should
  // be used instead.
  int base_path_key;

  // Explicit path to block instead of using |base_path_key|. Set to nullptr to
  // use |base_path_key| on its own. If both |base_path_key| and |path| are set,
  // |path| is treated relative to the path |base_path_key| resolves to.
  const base::FilePath::CharType* path;

  // If this is set to kDontBlockChildren, only the given path and its parents
  // are blocked. If this is set to kBlockAllChildren, all children of the given
  // path are blocked as well. Finally if this is set to kBlockNestedDirectories
  // access is allowed to individual files in the directory, but nested
  // directories are still blocked.
  // The BlockType of the nearest ancestor of a path to check is what ultimately
  // determines if a path is blocked or not. If a blocked path is a descendent
  // of another blocked path, then it may override the child-blocking policy of
  // its ancestor. For example, if /home blocks all children, but
  // /home/downloads does not, then /home/downloads/file.ext will *not* be
  // blocked.
  BlockType type;
} kBlockedPaths[] = {
    // Don't allow users to share their entire home directory, entire desktop or
    // entire documents folder, but do allow sharing anything inside those
    // directories not otherwise blocked.
    {base::DIR_HOME, nullptr, kDontBlockChildren},
    {base::DIR_USER_DESKTOP, nullptr, kDontBlockChildren},
    {chrome::DIR_USER_DOCUMENTS, nullptr, kDontBlockChildren},
    // Similar restrictions for the downloads directory.
    {chrome::DIR_DEFAULT_DOWNLOADS, nullptr, kDontBlockChildren},
    {chrome::DIR_DEFAULT_DOWNLOADS_SAFE, nullptr, kDontBlockChildren},
    // The Chrome installation itself should not be modified by the web.
    {chrome::DIR_APP, nullptr, kBlockAllChildren},
    // And neither should the configuration of at least the currently running
    // Chrome instance (note that this does not take --user-data-dir command
    // line overrides into account).
    {chrome::DIR_USER_DATA, nullptr, kBlockAllChildren},
    // ~/.ssh is pretty sensitive on all platforms, so block access to that.
    {base::DIR_HOME, FILE_PATH_LITERAL(".ssh"), kBlockAllChildren},
    // And limit access to ~/.gnupg as well.
    {base::DIR_HOME, FILE_PATH_LITERAL(".gnupg"), kBlockAllChildren},
#if defined(OS_WIN)
    // Some Windows specific directories to block, basically all apps, the
    // operating system itself, as well as configuration data for apps.
    {base::DIR_PROGRAM_FILES, nullptr, kBlockAllChildren},
    {base::DIR_PROGRAM_FILESX86, nullptr, kBlockAllChildren},
    {base::DIR_PROGRAM_FILES6432, nullptr, kBlockAllChildren},
    {base::DIR_WINDOWS, nullptr, kBlockAllChildren},
    {base::DIR_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_LOCAL_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_COMMON_APP_DATA, nullptr, kBlockAllChildren},
    // Opening a file from an MTP device, such as a smartphone or a camera, is
    // implemented by Windows as opening a file in the temporary internet files
    // directory. To support that, allow opening files in that directory, but
    // not whole directories.
    {base::DIR_IE_INTERNET_CACHE, nullptr, kBlockNestedDirectories},
#endif
#if defined(OS_MAC)
    // Similar Mac specific blocks.
    {base::DIR_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_HOME, FILE_PATH_LITERAL("Library"), kBlockAllChildren},
#endif
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    // On Linux also block access to devices via /dev, as well as security
    // sensitive data in /sys and /proc.
    {kNoBasePathKey, FILE_PATH_LITERAL("/dev"), kBlockAllChildren},
    {kNoBasePathKey, FILE_PATH_LITERAL("/sys"), kBlockAllChildren},
    {kNoBasePathKey, FILE_PATH_LITERAL("/proc"), kBlockAllChildren},
    // And block all of ~/.config, matching the similar restrictions on mac
    // and windows.
    {base::DIR_HOME, FILE_PATH_LITERAL(".config"), kBlockAllChildren},
    // Block ~/.dbus as well, just in case, although there probably isn't much a
    // website can do with access to that directory and its contents.
    {base::DIR_HOME, FILE_PATH_LITERAL(".dbus"), kBlockAllChildren},
#endif
    // TODO(https://crbug.com/984641): Refine this list, for example add
    // XDG_CONFIG_HOME when it is not set ~/.config?
};

bool ShouldBlockAccessToPath(const base::FilePath& check_path,
                             HandleType handle_type) {
  DCHECK(!check_path.empty());
  DCHECK(check_path.IsAbsolute());

  base::FilePath nearest_ancestor;
  int nearest_ancestor_path_key = kNoBasePathKey;
  BlockType nearest_ancestor_block_type = kDontBlockChildren;
  for (const auto& block : kBlockedPaths) {
    base::FilePath blocked_path;
    if (block.base_path_key != kNoBasePathKey) {
      if (!base::PathService::Get(block.base_path_key, &blocked_path))
        continue;
      if (block.path)
        blocked_path = blocked_path.Append(block.path);
    } else {
      DCHECK(block.path);
      blocked_path = base::FilePath(block.path);
    }

    if (check_path == blocked_path || check_path.IsParent(blocked_path)) {
      VLOG(1) << "Blocking access to " << check_path
              << " because it is a parent of " << blocked_path << " ("
              << block.base_path_key << ")";
      return true;
    }

    if (blocked_path.IsParent(check_path) &&
        (nearest_ancestor.empty() || nearest_ancestor.IsParent(blocked_path))) {
      nearest_ancestor = blocked_path;
      nearest_ancestor_path_key = block.base_path_key;
      nearest_ancestor_block_type = block.type;
    }
  }

  // The path we're checking is not in a potentially blocked directory, or the
  // nearest ancestor does not block access to its children. Grant access.
  if (nearest_ancestor.empty() ||
      nearest_ancestor_block_type == kDontBlockChildren) {
    return false;
  }

  // The path we're checking is a file, and the nearest ancestor only blocks
  // access to directories. Grant access.
  if (handle_type == HandleType::kFile &&
      nearest_ancestor_block_type == kBlockNestedDirectories) {
    return false;
  }

  // The nearest ancestor blocks access to its children, so block access.
  VLOG(1) << "Blocking access to " << check_path << " because it is inside "
          << nearest_ancestor << " (" << nearest_ancestor_path_key << ")";
  return true;
}

// Returns a callback that calls the passed in |callback| by posting a task to
// the current sequenced task runner.
template <typename... ResultTypes>
base::OnceCallback<void(ResultTypes... results)>
BindResultCallbackToCurrentSequence(
    base::OnceCallback<void(ResultTypes... results)> callback) {
  return base::BindOnce(
      [](scoped_refptr<base::TaskRunner> task_runner,
         base::OnceCallback<void(ResultTypes... results)> callback,
         ResultTypes... results) {
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), results...));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));
}

void DoSafeBrowsingCheckOnUIThread(
    content::GlobalFrameRoutingId frame_id,
    std::unique_ptr<content::NativeFileSystemWriteItem> item,
    safe_browsing::CheckDownloadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Download Protection Service is not supported on Android.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service || !sb_service->download_protection_service() ||
      !sb_service->download_protection_service()->enabled()) {
    std::move(callback).Run(safe_browsing::DownloadCheckResult::UNKNOWN);
    return;
  }

  if (!item->browser_context) {
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(frame_id.child_id);
    if (!rph) {
      std::move(callback).Run(safe_browsing::DownloadCheckResult::UNKNOWN);
      return;
    }
    item->browser_context = rph->GetBrowserContext();
  }

  if (!item->web_contents) {
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
    if (rfh)
      item->web_contents = content::WebContents::FromRenderFrameHost(rfh);
  }

  sb_service->download_protection_service()->CheckNativeFileSystemWrite(
      std::move(item), std::move(callback));
#endif
}

ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult
InterpretSafeBrowsingResult(safe_browsing::DownloadCheckResult result) {
  using Result = safe_browsing::DownloadCheckResult;
  switch (result) {
    // Only allow downloads that are marked as SAFE or UNKNOWN by SafeBrowsing.
    // All other types are going to be blocked. UNKNOWN could be the result of a
    // failed safe browsing ping.
    case Result::UNKNOWN:
    case Result::SAFE:
    case Result::ALLOWLISTED_BY_POLICY:
      return ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult::
          kAllow;

    case Result::DANGEROUS:
    case Result::UNCOMMON:
    case Result::DANGEROUS_HOST:
    case Result::POTENTIALLY_UNWANTED:
    case Result::BLOCKED_PASSWORD_PROTECTED:
    case Result::BLOCKED_TOO_LARGE:
    case Result::BLOCKED_UNSUPPORTED_FILE_TYPE:
      return ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult::
          kBlock;

    // This shouldn't be returned for Native File System write checks.
    case Result::ASYNC_SCANNING:
    case Result::SENSITIVE_CONTENT_WARNING:
    case Result::SENSITIVE_CONTENT_BLOCK:
    case Result::DEEP_SCANNED_SAFE:
    case Result::PROMPT_FOR_SCANNING:
      NOTREACHED();
      return ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult::
          kAllow;
  }
  NOTREACHED();
  return ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult::kBlock;
}

}  // namespace

ChromeNativeFileSystemPermissionContext::Grants::Grants() = default;
ChromeNativeFileSystemPermissionContext::Grants::~Grants() = default;
ChromeNativeFileSystemPermissionContext::Grants::Grants(Grants&&) = default;
ChromeNativeFileSystemPermissionContext::Grants&
ChromeNativeFileSystemPermissionContext::Grants::operator=(Grants&&) = default;

ChromeNativeFileSystemPermissionContext::
    ChromeNativeFileSystemPermissionContext(content::BrowserContext* context) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  auto* profile = Profile::FromBrowserContext(context);
  content_settings_ = base::WrapRefCounted(
      HostContentSettingsMapFactory::GetForProfile(profile));
}

ChromeNativeFileSystemPermissionContext::
    ~ChromeNativeFileSystemPermissionContext() = default;

ContentSetting
ChromeNativeFileSystemPermissionContext::GetWriteGuardContentSetting(
    const url::Origin& origin) {
  return content_settings()->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
}

ContentSetting
ChromeNativeFileSystemPermissionContext::GetReadGuardContentSetting(
    const url::Origin& origin) {
  return content_settings()->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_READ_GUARD);
}

bool ChromeNativeFileSystemPermissionContext::CanObtainReadPermission(
    const url::Origin& origin) {
  return GetReadGuardContentSetting(origin) == CONTENT_SETTING_ASK ||
         GetReadGuardContentSetting(origin) == CONTENT_SETTING_ALLOW;
}

bool ChromeNativeFileSystemPermissionContext::CanObtainWritePermission(
    const url::Origin& origin) {
  return GetWriteGuardContentSetting(origin) == CONTENT_SETTING_ASK ||
         GetWriteGuardContentSetting(origin) == CONTENT_SETTING_ALLOW;
}

void ChromeNativeFileSystemPermissionContext::ConfirmSensitiveDirectoryAccess(
    const url::Origin& origin,
    PathType path_type,
    const base::FilePath& path,
    HandleType handle_type,
    content::GlobalFrameRoutingId frame_id,
    base::OnceCallback<void(SensitiveDirectoryResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/1009970): Figure out what external paths should be
  // blocked. We could resolve the external path to a local path, and check for
  // blocked directories based on that, but that doesn't work well. Instead we
  // should have a separate Chrome OS only code path to block for example the
  // root of certain external file systems.
  if (path_type == PathType::kExternal) {
    std::move(callback).Run(SensitiveDirectoryResult::kAllowed);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ShouldBlockAccessToPath, path, handle_type),
      base::BindOnce(&ChromeNativeFileSystemPermissionContext::
                         DidConfirmSensitiveDirectoryAccess,
                     GetWeakPtr(), origin, path, handle_type, frame_id,
                     std::move(callback)));
}

void ChromeNativeFileSystemPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<content::NativeFileSystemWriteItem> item,
    content::GlobalFrameRoutingId frame_id,
    base::OnceCallback<void(AfterWriteCheckResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DoSafeBrowsingCheckOnUIThread, frame_id, std::move(item),
          base::BindOnce(
              [](scoped_refptr<base::TaskRunner> task_runner,
                 base::OnceCallback<void(AfterWriteCheckResult result)>
                     callback,
                 safe_browsing::DownloadCheckResult result) {
                task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback),
                                   InterpretSafeBrowsingResult(result)));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void ChromeNativeFileSystemPermissionContext::
    DidConfirmSensitiveDirectoryAccess(
        const url::Origin& origin,
        const base::FilePath& path,
        HandleType handle_type,
        content::GlobalFrameRoutingId frame_id,
        base::OnceCallback<void(SensitiveDirectoryResult)> callback,
        bool should_block) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!should_block) {
    std::move(callback).Run(SensitiveDirectoryResult::kAllowed);
    return;
  }

  auto result_callback =
      BindResultCallbackToCurrentSequence(std::move(callback));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ShowNativeFileSystemRestrictedDirectoryDialogOnUIThread,
                     frame_id, origin, path, handle_type,
                     std::move(result_callback)));
}

bool ChromeNativeFileSystemPermissionContext::OriginHasReadAccess(
    const url::Origin& origin) {
  NOTREACHED();
  return false;
}

bool ChromeNativeFileSystemPermissionContext::OriginHasWriteAccess(
    const url::Origin& origin) {
  NOTREACHED();
  return false;
}

void ChromeNativeFileSystemPermissionContext::SetLastPickedDirectory(
    const url::Origin& origin,
    const base::FilePath& path,
    const PathType type) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kLastPickedDirectoryKey, util::FilePathToValue(path));
  dict.SetIntKey(kLastPickedDirectoryTypeKey, static_cast<int>(type));

  content_settings_->SetWebsiteSettingDefaultScope(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
      base::Value::ToUniquePtrValue(std::move(dict)));
}

ChromeNativeFileSystemPermissionContext::PathInfo
ChromeNativeFileSystemPermissionContext::GetLastPickedDirectory(
    const url::Origin& origin) {
  std::unique_ptr<base::Value> value = content_settings()->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY, /*info=*/nullptr);

  PathInfo path_info;
  if (!value)
    return path_info;

  auto type_int = value->FindIntKey(kLastPickedDirectoryTypeKey)
                      .value_or(static_cast<int>(PathType::kLocal));
  path_info.type = type_int == static_cast<int>(PathType::kExternal)
                       ? PathType::kExternal
                       : PathType::kLocal;
  path_info.path =
      util::ValueToFilePath(value->FindKey(kLastPickedDirectoryKey))
          .value_or(base::FilePath());

  return path_info;
}

base::FilePath ChromeNativeFileSystemPermissionContext::GetCommonDirectoryPath(
    blink::mojom::CommonDirectory directory) {
  int key = base::PATH_START;
  switch (directory) {
    case blink::mojom::CommonDirectory::kDefault:
      key = chrome::DIR_USER_DOCUMENTS;
      break;
    case blink::mojom::CommonDirectory::kDirDesktop:
      key = base::DIR_USER_DESKTOP;
      break;
    case blink::mojom::CommonDirectory::kDirDocuments:
      key = chrome::DIR_USER_DOCUMENTS;
      break;
    case blink::mojom::CommonDirectory::kDirDownloads:
      key = chrome::DIR_DEFAULT_DOWNLOADS;
      break;
    case blink::mojom::CommonDirectory::kDirHome:
      key = base::DIR_HOME;
      break;
    case blink::mojom::CommonDirectory::kDirMusic:
      key = chrome::DIR_USER_MUSIC;
      break;
    case blink::mojom::CommonDirectory::kDirPictures:
      key = chrome::DIR_USER_PICTURES;
      break;
    case blink::mojom::CommonDirectory::kDirVideos:
      key = chrome::DIR_USER_VIDEOS;
      break;
  }
  base::FilePath directory_path;
  base::PathService::Get(key, &directory_path);
  return directory_path;
}
