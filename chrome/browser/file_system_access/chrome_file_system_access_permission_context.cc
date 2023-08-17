// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/ui/file_system_access_dialogs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pdf_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_util.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#endif

namespace features {
BASE_FEATURE(kFileSystemAccessPersistentPermissions,
             "kFileSystemAccessPersistentPermissions",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kFileSystemAccessLocalUNCPathBlock,
             "kFileSystemAccessLocalUNCPathBlock",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif
}  // namespace features

namespace {

using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using GrantType = ChromeFileSystemAccessPermissionContext::GrantType;
using blink::mojom::PermissionStatus;
using permissions::PermissionAction;

// This long after the last top-level tab or window for an origin is closed (or
// is navigated to another origin), all the permissions for that origin will be
// revoked.
constexpr base::TimeDelta kPermissionRevocationTimeout = base::Seconds(5);

// Dictionary keys for the FILE_SYSTEM_ACCESS_CHOOSER_DATA setting.
// `kPermissionPathKey[] = "path"` is defined in the header file.
const char kPermissionIsDirectoryKey[] = "is-directory";
const char kPermissionWritableKey[] = "writable";
const char kPermissionReadableKey[] = "readable";
const char kDeprecatedPermissionLastUsedTimeKey[] = "time";

// Dictionary keys for the FILE_SYSTEM_LAST_PICKED_DIRECTORY website setting.
// Schema (per origin):
// {
//  ...
//   {
//     "default-id" : { "path" : <path> , "path-type" : <type>}
//     "custom-id-fruit" : { "path" : <path> , "path-type" : <type> }
//     "custom-id-flower" : { "path" : <path> , "path-type" : <type> }
//     ...
//   }
//  ...
// }
const char kDefaultLastPickedDirectoryKey[] = "default-id";
const char kCustomLastPickedDirectoryKey[] = "custom-id";
const char kPathKey[] = "path";
const char kPathTypeKey[] = "path-type";
const char kTimestampKey[] = "timestamp";

void ShowFileSystemAccessRestrictedDirectoryDialogOnUIThread(
    content::GlobalRenderFrameHostId frame_id,
    const url::Origin& origin,
    HandleType handle_type,
    base::OnceCallback<
        void(ChromeFileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
  if (!rfh || !rfh->IsActive()) {
    // Requested from a no longer valid RenderFrameHost.
    std::move(callback).Run(
        ChromeFileSystemAccessPermissionContext::SensitiveEntryResult::kAbort);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(
        ChromeFileSystemAccessPermissionContext::SensitiveEntryResult::kAbort);
    return;
  }

  ShowFileSystemAccessRestrictedDirectoryDialog(
      origin, handle_type, std::move(callback), web_contents);
}

void ShowFileSystemAccessDangerousFileDialogOnUIThread(
    content::GlobalRenderFrameHostId frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<
        void(ChromeFileSystemAccessPermissionContext::SensitiveEntryResult)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
  if (!rfh || !rfh->IsActive()) {
    // Requested from a no longer valid RenderFrameHost.
    std::move(callback).Run(
        ChromeFileSystemAccessPermissionContext::SensitiveEntryResult::kAbort);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(
        ChromeFileSystemAccessPermissionContext::SensitiveEntryResult::kAbort);
    return;
  }

  ShowFileSystemAccessDangerousFileDialog(origin, path, std::move(callback),
                                          web_contents);
}

#if BUILDFLAG(IS_WIN)
bool ContainsInvalidDNSCharacter(base::FilePath::StringType hostname) {
  for (base::FilePath::CharType c : hostname) {
    if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
          (c >= L'0' && c <= L'9') || (c == L'.') || (c == L'-'))) {
      return true;
    }
  }
  return false;
}

bool MaybeIsLocalUNCPath(const base::FilePath& path) {
  if (!path.IsNetwork()) {
    return false;
  }

  const std::vector<base::FilePath::StringType> components =
      path.GetComponents();

  // Check for server name that could represent a local system. We only
  // check for a very short list, as it is impossible to cover all different
  // variants on Windows.
  if (components.size() >= 2 &&
      (base::FilePath::CompareEqualIgnoreCase(components[1],
                                              FILE_PATH_LITERAL("localhost")) ||
       components[1] == FILE_PATH_LITERAL("127.0.0.1") ||
       components[1] == FILE_PATH_LITERAL(".") ||
       components[1] == FILE_PATH_LITERAL("?") ||
       ContainsInvalidDNSCharacter(components[1]))) {
    return true;
  }

  // In case we missed the server name check above, we also check for shares
  // ending with '$' as they represent pre-defined shares, including the local
  // drives.
  for (size_t i = 2; i < components.size(); ++i) {
    if (components[i].back() == L'$') {
      return true;
    }
  }

  return false;
}
#endif

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
    {base::DIR_EXE, nullptr, kBlockAllChildren},
#if !BUILDFLAG(IS_FUCHSIA)
    {base::DIR_MODULE, nullptr, kBlockAllChildren},
#endif
    {base::DIR_ASSETS, nullptr, kBlockAllChildren},
    // And neither should the configuration of at least the currently running
    // Chrome instance (note that this does not take --user-data-dir command
    // line overrides into account).
    {chrome::DIR_USER_DATA, nullptr, kBlockAllChildren},
    // ~/.ssh is pretty sensitive on all platforms, so block access to that.
    {base::DIR_HOME, FILE_PATH_LITERAL(".ssh"), kBlockAllChildren},
    // And limit access to ~/.gnupg as well.
    {base::DIR_HOME, FILE_PATH_LITERAL(".gnupg"), kBlockAllChildren},
#if BUILDFLAG(IS_WIN)
    // Some Windows specific directories to block, basically all apps, the
    // operating system itself, as well as configuration data for apps.
    {base::DIR_PROGRAM_FILES, nullptr, kBlockAllChildren},
    {base::DIR_PROGRAM_FILESX86, nullptr, kBlockAllChildren},
    {base::DIR_PROGRAM_FILES6432, nullptr, kBlockAllChildren},
    {base::DIR_WINDOWS, nullptr, kBlockAllChildren},
    {base::DIR_ROAMING_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_LOCAL_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_COMMON_APP_DATA, nullptr, kBlockAllChildren},
    // Opening a file from an MTP device, such as a smartphone or a camera, is
    // implemented by Windows as opening a file in the temporary internet files
    // directory. To support that, allow opening files in that directory, but
    // not whole directories.
    {base::DIR_IE_INTERNET_CACHE, nullptr, kBlockNestedDirectories},
#endif
#if BUILDFLAG(IS_MAC)
    // Similar Mac specific blocks.
    {base::DIR_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_HOME, FILE_PATH_LITERAL("Library"), kBlockAllChildren},
    // Allow access to other cloud files, such as Google Drive.
    {base::DIR_HOME, FILE_PATH_LITERAL("Library/CloudStorage"),
     kDontBlockChildren},
    // Allow the site to interact with data from its corresponding natively
    // installed (sandboxed) application. It would be nice to limit a site to
    // access only _its_ corresponding natively installed application,
    // but unfortunately there's no straightforward way to do that. See
    // https://crbug.com/984641#c22.
    {base::DIR_HOME, FILE_PATH_LITERAL("Library/Containers"),
     kDontBlockChildren},
    // Allow access to iCloud files...
    {base::DIR_HOME, FILE_PATH_LITERAL("Library/Mobile Documents"),
     kDontBlockChildren},
    // ... which may also appear at this directory.
    {base::DIR_HOME,
     FILE_PATH_LITERAL("Library/Mobile Documents/com~apple~CloudDocs"),
     kDontBlockChildren},
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // On Linux also block access to devices via /dev.
    {kNoBasePathKey, FILE_PATH_LITERAL("/dev"), kBlockAllChildren},
    // And security sensitive data in /proc and /sys.
    {kNoBasePathKey, FILE_PATH_LITERAL("/proc"), kBlockAllChildren},
    {kNoBasePathKey, FILE_PATH_LITERAL("/sys"), kBlockAllChildren},
    // And system files in /boot and /etc.
    {kNoBasePathKey, FILE_PATH_LITERAL("/boot"), kBlockAllChildren},
    {kNoBasePathKey, FILE_PATH_LITERAL("/etc"), kBlockAllChildren},
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

#if BUILDFLAG(IS_WIN)
  // On Windows, local UNC paths are rejected, as UNC path can be written in a
  // way that can bypass the blocklist.
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessLocalUNCPathBlock) &&
      MaybeIsLocalUNCPath(check_path)) {
    return true;
  }
#endif

  base::FilePath nearest_ancestor;
  int nearest_ancestor_path_key = kNoBasePathKey;
  BlockType nearest_ancestor_block_type = kDontBlockChildren;
  for (const auto& block : kBlockedPaths) {
    base::FilePath blocked_path;
    if (block.base_path_key != kNoBasePathKey) {
      if (!base::PathService::Get(block.base_path_key, &blocked_path)) {
        continue;
      }
      if (block.path) {
        blocked_path = blocked_path.Append(block.path);
      }
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

void DoSafeBrowsingCheckOnUIThread(
    content::GlobalRenderFrameHostId frame_id,
    std::unique_ptr<content::FileSystemAccessWriteItem> item,
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
    if (rfh) {
      DCHECK_NE(rfh->GetLifecycleState(),
                content::RenderFrameHost::LifecycleState::kPrerendering);
      item->web_contents = content::WebContents::FromRenderFrameHost(rfh);
    }
  }

  sb_service->download_protection_service()->CheckFileSystemAccessWrite(
      std::move(item), std::move(callback));
#else
  std::move(callback).Run(safe_browsing::DownloadCheckResult::UNKNOWN);
#endif
}

ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult
InterpretSafeBrowsingResult(safe_browsing::DownloadCheckResult result) {
  using Result = safe_browsing::DownloadCheckResult;
  switch (result) {
    // Only allow downloads that are marked as SAFE or UNKNOWN by SafeBrowsing.
    // All other types are going to be blocked. UNKNOWN could be the result of a
    // failed safe browsing ping or if Safe Browsing is not enabled.
    case Result::UNKNOWN:
    case Result::SAFE:
    case Result::ALLOWLISTED_BY_POLICY:
      return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::
          kAllow;

    case Result::DANGEROUS:
    case Result::UNCOMMON:
    case Result::DANGEROUS_HOST:
    case Result::POTENTIALLY_UNWANTED:
    case Result::BLOCKED_PASSWORD_PROTECTED:
    case Result::BLOCKED_TOO_LARGE:
    case Result::BLOCKED_UNSUPPORTED_FILE_TYPE:
    case Result::DANGEROUS_ACCOUNT_COMPROMISE:
      return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::
          kBlock;

    // This shouldn't be returned for File System Access write checks.
    case Result::ASYNC_SCANNING:
    case Result::SENSITIVE_CONTENT_WARNING:
    case Result::SENSITIVE_CONTENT_BLOCK:
    case Result::DEEP_SCANNED_SAFE:
    case Result::PROMPT_FOR_SCANNING:
    case Result::DEEP_SCANNED_FAILED:
      NOTREACHED();
      return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::
          kAllow;
  }
  NOTREACHED();
  return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::kBlock;
}

std::string GenerateLastPickedDirectoryKey(const std::string& id) {
  return id.empty() ? kDefaultLastPickedDirectoryKey
                    : base::StrCat({kCustomLastPickedDirectoryKey, "-", id});
}

std::string_view PathAsPermissionKey(const base::FilePath& path) {
  return std::string_view(
      reinterpret_cast<const char*>(path.value().data()),
      path.value().size() * sizeof(base::FilePath::CharType));
}

std::string_view GetGrantKeyFromGrantType(GrantType type) {
  return type == GrantType::kWrite ? kPermissionWritableKey
                                   : kPermissionReadableKey;
}

bool FileHasDangerousExtension(const url::Origin& origin,
                               const base::FilePath& path,
                               Profile* profile) {
  safe_browsing::DownloadFileType::DangerLevel danger_level =
      safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
          path, origin.GetURL(), profile->GetPrefs());
  // See https://crbug.com/1320877#c4 for justification for why we show the
  // prompt if `danger_level` is ALLOW_ON_USER_GESTURE as well as DANGEROUS.
  return danger_level == safe_browsing::DownloadFileType::DANGEROUS ||
         danger_level == safe_browsing::DownloadFileType::ALLOW_ON_USER_GESTURE;
}

}  // namespace

ChromeFileSystemAccessPermissionContext::Grants::Grants() = default;
ChromeFileSystemAccessPermissionContext::Grants::~Grants() = default;
ChromeFileSystemAccessPermissionContext::Grants::Grants(Grants&&) = default;
ChromeFileSystemAccessPermissionContext::Grants&
ChromeFileSystemAccessPermissionContext::Grants::operator=(Grants&&) = default;

class ChromeFileSystemAccessPermissionContext::PermissionGrantImpl
    : public content::FileSystemAccessPermissionGrant {
 public:
  PermissionGrantImpl(
      base::WeakPtr<ChromeFileSystemAccessPermissionContext> context,
      const url::Origin& origin,
      const base::FilePath& path,
      HandleType handle_type,
      GrantType type,
      UserAction user_action)
      : context_(std::move(context)),
        origin_(origin),
        handle_type_(handle_type),
        type_(type),
        path_(path) {}

  // FileSystemAccessPermissionGrant:
  PermissionStatus GetStatus() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return status_;
  }
  base::FilePath GetPath() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_;
  }

  void RequestPermission(
      content::GlobalRenderFrameHostId frame_id,
      UserActivationState user_activation_state,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Check if a permission request has already been processed previously. This
    // check is done first because we don't want to reset the status of a
    // permission if it has already been granted.
    if (GetStatus() != PermissionStatus::ASK || !context_) {
      if (GetStatus() == PermissionStatus::GRANTED) {
        SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
      }
      std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
      return;
    }

    if (HasExtendedPermission()) {
      SetStatus(PermissionStatus::GRANTED,
                PersistedPermissionOptions::kUpdatePersistedPermission);
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback),
          PermissionRequestOutcome::kGrantedByPersistentPermission);
      return;
    }

    if (AncestorHasExtendedPermission()) {
      SetStatus(PermissionStatus::GRANTED,
                PersistedPermissionOptions::kUpdatePersistedPermission);
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback),
          PermissionRequestOutcome::kGrantedByAncestorPersistentPermission);
      return;
    }

    if (type_ == GrantType::kWrite) {
      ContentSetting content_setting =
          context_->GetWriteGuardContentSetting(origin_);

      // Content setting grants write permission without asking.
      if (content_setting == CONTENT_SETTING_ALLOW) {
        SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback),
            PermissionRequestOutcome::kGrantedByContentSetting);
        return;
      }

      // Content setting blocks write permission.
      if (content_setting == CONTENT_SETTING_BLOCK) {
        SetStatus(PermissionStatus::DENIED,
                  PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback),
            PermissionRequestOutcome::kBlockedByContentSetting);
        return;
      }
    }

    // Otherwise, perform checks and ask the user for permission.

    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
    if (!rfh) {
      // Requested from a no longer valid RenderFrameHost.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }

    // Don't show request permission UI for an inactive RenderFrameHost as the
    // page might not distinguish properly between user denying the permission
    // and automatic rejection, leading to an inconsistent UX once the page
    // becomes active again.
    // - If this is called when RenderFrameHost is in BackForwardCache, evict
    //   the document from the cache.
    // - If this is called when RenderFrameHost is in prerendering, cancel
    //   prerendering.
    if (rfh->IsInactiveAndDisallowActivation(
            content::DisallowActivationReasonId::
                kFileSystemAccessPermissionRequest)) {
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }
    // We don't allow file system access from fenced frames.
    if (rfh->IsNestedWithinFencedFrame()) {
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }

    if (user_activation_state == UserActivationState::kRequired &&
        !rfh->HasTransientUserActivation()) {
      // No permission prompts without user activation.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kNoUserActivation);
      return;
    }

    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    if (!web_contents) {
      // Requested from a worker, or a no longer existing tab.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }

    url::Origin embedding_origin = url::Origin::Create(
        permissions::PermissionUtil::GetLastCommittedOriginAsURL(
            rfh->GetMainFrame()));
    if (embedding_origin != origin_) {
      // Third party iframes are not allowed to request more permissions.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kThirdPartyContext);
      return;
    }

    auto* request_manager =
        FileSystemAccessPermissionRequestManager::FromWebContents(web_contents);
    if (!request_manager) {
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kRequestAborted);
      return;
    }

    // Drop fullscreen mode so that the user sees the URL bar.
    base::ScopedClosureRunner fullscreen_block =
        web_contents->ForSecurityDropFullscreen();

    FileSystemAccessPermissionRequestManager::Access access =
        type_ == GrantType::kRead
            ? FileSystemAccessPermissionRequestManager::Access::kRead
            : FileSystemAccessPermissionRequestManager::Access::kWrite;

    // If a website wants both read and write access, code in content will
    // request those as two separate requests. The |request_manager| will then
    // detect this and combine the two requests into one prompt. As such this
    // code does not have to have any way to request Access::kReadWrite.

    request_manager->AddRequest(
        {origin_, path_, handle_type_, access},
        base::BindOnce(&PermissionGrantImpl::OnPermissionRequestResult, this,
                       std::move(callback)),
        std::move(fullscreen_block));
  }

  bool HasExtendedPermission() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return context_->HasExtendedPermission(origin_, path_, handle_type_, type_);
  }

  bool AncestorHasExtendedPermission() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (base::FilePath parent = path_.DirName(); parent != parent.DirName();
         parent = parent.DirName()) {
      if (context_->HasExtendedPermission(origin_, parent,
                                          HandleType::kDirectory, type_)) {
        return true;
      }
    }
    return false;
  }

  const url::Origin& origin() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return origin_;
  }

  HandleType handle_type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return handle_type_;
  }

  GrantType type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return type_;
  }

  void SetStatus(PermissionStatus status,
                 PersistedPermissionOptions persisted_status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool should_notify = status_ != status;
    status_ = status;

    if (context_ &&
        persisted_status ==
            PersistedPermissionOptions::kUpdatePersistedPermission &&
        base::FeatureList::IsEnabled(
            features::kFileSystemAccessPersistentPermissions)) {
      if (status == PermissionStatus::GRANTED) {
        const std::unique_ptr<Object> object =
            context_->GetGrantedObject(origin_, PathAsPermissionKey(path_));
        if (object) {
          // Persisted permissions include both read and write information in
          // one object. Figure out if the other grant type is already
          // persisted and update the existing one.
          auto opposite_type =
              type_ == GrantType::kRead ? GrantType::kWrite : GrantType::kRead;
          auto type_exists =
              object->value.FindBool(GetGrantKeyFromGrantType(type_))
                  .value_or(false);
          auto opposite_type_exists =
              object->value.FindBool(GetGrantKeyFromGrantType(opposite_type))
                  .value_or(false);
          if (!type_exists && opposite_type_exists) {
            base::Value::Dict new_object = object->value.Clone();
            new_object.Set(GetGrantKeyFromGrantType(type_), true);
            context_->UpdateObjectPermission(origin_, object->value,
                                             std::move(new_object));
          }
        } else {
          base::Value::Dict grant = AsValue();
          context_->GrantObjectPermission(origin_, std::move(grant));
        }
      } else {
        context_->RevokeObjectPermission(origin_, GetKey());
      }
    }
    if (should_notify) {
      NotifyPermissionStatusChanged();
    }
  }

  base::Value::Dict AsValue() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::Value::Dict value;
    value.Set(kPermissionPathKey, base::FilePathToValue(path_));
    value.Set(kPermissionIsDirectoryKey,
              handle_type_ == HandleType::kDirectory);
    value.Set(GetGrantKeyFromGrantType(type_), true);

    // Persisted permissions include both read and write information in one
    // object. Figure out if the other grant type is already persisted.
    auto opposite_type =
        type_ == GrantType::kRead ? GrantType::kWrite : GrantType::kRead;
    if (context_->HasExtendedPermission(origin_, path_, handle_type_,
                                        opposite_type)) {
      value.Set(GetGrantKeyFromGrantType(opposite_type), true);
    }
    return value;
  }

  static void UpdateGrantPath(
      std::map<base::FilePath, PermissionGrantImpl*>& grants,
      const base::FilePath& old_path,
      const base::FilePath& new_path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto entry_it = base::ranges::find_if(
        grants,
        [&old_path](const auto& entry) { return entry.first == old_path; });

    if (entry_it == grants.end()) {
      // There must be an entry for an ancestor of this entry. Nothing to do
      // here.
      //
      // TODO(https://crbug.com/1381302): Consolidate superfluous child grants
      // to support directory moves.
      return;
    }

    DCHECK_EQ(entry_it->second->GetStatus(), PermissionStatus::GRANTED);

    auto* const grant_impl = entry_it->second;
    grant_impl->SetPath(new_path);

    // Update the permission grant's key in the map of active permissions.
    grants.erase(entry_it);
    grants.emplace(new_path, grant_impl);
  }

 protected:
  ~PermissionGrantImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (context_) {
      context_->PermissionGrantDestroyed(this);
    }
  }

 private:
  void OnPermissionRequestResult(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionAction result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    switch (result) {
      case PermissionAction::GRANTED:
        SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserGranted);
        if (context_) {
          context_->ScheduleUsageIconUpdate();
        }
        break;
      case PermissionAction::DENIED:
        SetStatus(PermissionStatus::DENIED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDenied);
        break;
      case PermissionAction::DISMISSED:
      case PermissionAction::IGNORED:
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDismissed);
        break;
      case PermissionAction::REVOKED:
      case PermissionAction::GRANTED_ONCE:
      case PermissionAction::NUM:
        NOTREACHED();
        break;
    }
  }

  void RunCallbackAndRecordPermissionRequestOutcome(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionRequestOutcome outcome) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (type_ == GrantType::kWrite) {
      base::UmaHistogramEnumeration(
          "Storage.FileSystemAccess.WritePermissionRequestOutcome", outcome);
      if (handle_type_ == HandleType::kDirectory) {
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.WritePermissionRequestOutcome.Directory",
            outcome);
      } else {
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.WritePermissionRequestOutcome.File",
            outcome);
      }
    } else {
      base::UmaHistogramEnumeration(
          "Storage.FileSystemAccess.ReadPermissionRequestOutcome", outcome);
      if (handle_type_ == HandleType::kDirectory) {
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.ReadPermissionRequestOutcome.Directory",
            outcome);
      } else {
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.ReadPermissionRequestOutcome.File",
            outcome);
      }
    }

    std::move(callback).Run(outcome);
  }

  std::string_view GetKey() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return PathAsPermissionKey(path_);
  }

  // TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
  // feature flag checks before launch.
  void SetPath(const base::FilePath& new_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (path_ == new_path) {
      return;
    }

    if (base::FeatureList::IsEnabled(
            features::kFileSystemAccessPersistentPermissions)) {
      const auto& grant_before = AsValue();
      path_ = new_path;
      // If there is an existing dormant or extended object, update with the
      // new object.
      context_->UpdateObjectPermission(origin_, grant_before, AsValue());
    } else {
      path_ = new_path;
    }

    NotifyPermissionStatusChanged();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<ChromeFileSystemAccessPermissionContext> const context_;
  const url::Origin origin_;
  const HandleType handle_type_;
  const GrantType type_;
  // `path_` can be updated if the entry is moved.
  base::FilePath path_;
  base::Time last_used_time_;

  // This member should only be updated via SetStatus(), to make sure
  // observers are properly notified about any change in status.
  PermissionStatus status_ = PermissionStatus::ASK;
};

struct ChromeFileSystemAccessPermissionContext::OriginState {
  // Raw pointers, owned collectively by all the handles that reference this
  // grant. When last reference goes away this state is cleared as well by
  // PermissionGrantDestroyed().
  std::map<base::FilePath, PermissionGrantImpl*> read_grants;
  std::map<base::FilePath, PermissionGrantImpl*> write_grants;

  // Timer that is triggered whenever the user navigates away from this origin.
  // This is used to give a website a little bit of time for background work
  // before revoking all permissions for the origin.
  std::unique_ptr<base::RetainingOneShotTimer> cleanup_timer;
};

ChromeFileSystemAccessPermissionContext::
    ChromeFileSystemAccessPermissionContext(content::BrowserContext* context,
                                            const base::Clock* clock)
    : ObjectPermissionContextBase(
          ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
          ContentSettingsType::FILE_SYSTEM_ACCESS_CHOOSER_DATA,
          HostContentSettingsMapFactory::GetForProfile(context)),
      profile_(context),
      clock_(clock) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  content_settings_ = base::WrapRefCounted(
      HostContentSettingsMapFactory::GetForProfile(profile_));

  // TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
  // feature flag checks before launch.
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    // Deprecated persisted permission objects contains a timestamp key, used
    // in old implementation. Revoke them so that the state is reset for the new
    // persisted permission implementation.
    std::set<url::Origin> origins =
        ObjectPermissionContextBase::GetOriginsWithGrants();
    for (auto& origin : origins) {
      for (auto& object :
           ObjectPermissionContextBase::GetGrantedObjects(origin)) {
        if (object->value.contains(kDeprecatedPermissionLastUsedTimeKey)) {
          RevokeObjectPermission(origin, GetKeyForObject(object->value));
        }
      }
    }
  }
}

ChromeFileSystemAccessPermissionContext::
    ~ChromeFileSystemAccessPermissionContext() = default;

bool ChromeFileSystemAccessPermissionContext::RevokeActiveGrants(
    const url::Origin& origin,
    base::FilePath file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool grant_revoked = false;

  auto origin_it = active_permissions_map_.find(origin);
  if (origin_it != active_permissions_map_.end()) {
    OriginState& origin_state = origin_it->second;
    for (auto& grant : origin_state.read_grants) {
      if (file_path.empty() || grant.first.value() == file_path.value()) {
        grant.second->SetStatus(
            PermissionStatus::ASK,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
        grant_revoked = true;
      }
    }
    for (auto& grant : origin_state.write_grants) {
      if (file_path.empty() || grant.first.value() == file_path.value()) {
        grant.second->SetStatus(
            PermissionStatus::ASK,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
        grant_revoked = true;
      }
    }
  }
  return grant_revoked;
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |active_permissions_map_|,
  // but that is exactly what we want.
  auto& origin_state = active_permissions_map_[origin];
  auto*& existing_grant = origin_state.read_grants[path];
  scoped_refptr<PermissionGrantImpl> new_grant;

  if (existing_grant && existing_grant->handle_type() != handle_type) {
    // |path| changed from being a directory to being a file or vice versa,
    // don't just re-use the existing grant but revoke the old grant before
    // creating a new grant.
    existing_grant->SetStatus(
        PermissionStatus::DENIED,
        PersistedPermissionOptions::kUpdatePersistedPermission);
    existing_grant = nullptr;
  }

  if (!existing_grant) {
    new_grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path, handle_type, GrantType::kRead,
        user_action);
    existing_grant = new_grant.get();
  }

  const ContentSetting content_setting = GetReadGuardContentSetting(origin);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      // Don't persist permissions when the origin is allowlisted.
      existing_grant->SetStatus(
          PermissionStatus::GRANTED,
          PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      break;
    case CONTENT_SETTING_ASK:
      // If a parent directory is already readable this new grant should also be
      // readable.
      if (new_grant &&
          AncestorHasActivePermission(origin, path, GrantType::kRead)) {
        existing_grant->SetStatus(
            PermissionStatus::GRANTED,
            PersistedPermissionOptions::kUpdatePersistedPermission);
        break;
      }
      switch (user_action) {
        case UserAction::kOpen:
        case UserAction::kSave:
          // Open and Save dialog only grant read access for individual files.
          if (handle_type == HandleType::kDirectory) {
            break;
          }
          [[fallthrough]];
        case UserAction::kDragAndDrop:
          // Drag&drop grants read access for all handles.
          existing_grant->SetStatus(
              PermissionStatus::GRANTED,
              PersistedPermissionOptions::kUpdatePersistedPermission);
          break;
        case UserAction::kLoadFromStorage:
        case UserAction::kNone:
          break;
      }
      break;
    case CONTENT_SETTING_BLOCK:
      // Don't bother revoking persisted permissions. If the permissions have
      // not yet expired when the ContentSettingValue is changed, they will
      // effectively be reinstated.
      if (new_grant) {
        existing_grant->SetStatus(
            PermissionStatus::DENIED,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      } else {
        // We won't revoke permission to an existing grant.
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  if (existing_grant->GetStatus() == PermissionStatus::GRANTED) {
    ScheduleUsageIconUpdate();
  }

  return existing_grant;
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |active_permissions_map_|,
  // but that is exactly what we want.
  auto& origin_state = active_permissions_map_[origin];
  auto*& existing_grant = origin_state.write_grants[path];
  scoped_refptr<PermissionGrantImpl> new_grant;

  if (existing_grant && existing_grant->handle_type() != handle_type) {
    // |path| changed from being a directory to being a file or vice versa,
    // don't just re-use the existing grant but revoke the old grant before
    // creating a new grant.
    existing_grant->SetStatus(
        PermissionStatus::DENIED,
        PersistedPermissionOptions::kUpdatePersistedPermission);
    existing_grant = nullptr;
  }

  if (!existing_grant) {
    new_grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path, handle_type,
        GrantType::kWrite, user_action);
    existing_grant = new_grant.get();
  }

  const ContentSetting content_setting = GetWriteGuardContentSetting(origin);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      // Don't persist permissions when the origin is allowlisted.
      existing_grant->SetStatus(
          PermissionStatus::GRANTED,
          PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      break;
    case CONTENT_SETTING_ASK:
      // If a parent directory is already writable this new grant should also be
      // writable.
      if (new_grant &&
          AncestorHasActivePermission(origin, path, GrantType::kWrite)) {
        existing_grant->SetStatus(
            PermissionStatus::GRANTED,
            PersistedPermissionOptions::kUpdatePersistedPermission);
        break;
      }
      switch (user_action) {
        case UserAction::kSave:
          // Only automatically grant write access for save dialogs.
          existing_grant->SetStatus(
              PermissionStatus::GRANTED,
              PersistedPermissionOptions::kUpdatePersistedPermission);
          break;
        case UserAction::kOpen:
        case UserAction::kDragAndDrop:
        case UserAction::kLoadFromStorage:
        case UserAction::kNone:
          break;
      }
      break;
    case CONTENT_SETTING_BLOCK:
      // Don't bother revoking persisted permissions. If the permissions have
      // not yet expired when the ContentSettingValue is changed, they will
      // effectively be reinstated.
      if (new_grant) {
        existing_grant->SetStatus(
            PermissionStatus::DENIED,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      } else {
        // We won't revoke permission to an existing grant.
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  if (existing_grant->GetStatus() == PermissionStatus::GRANTED) {
    ScheduleUsageIconUpdate();
  }

  return existing_grant;
}

// Return extended permission grants for an origin.
std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
ChromeFileSystemAccessPermissionContext::GetExtendedPersistedObjects(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetPersistedGrantState(origin) == PersistedGrantState::kExtended) {
    // When the origin has extended permission enabled, all permissions objects
    // represent extended grants.
    return ObjectPermissionContextBase::GetGrantedObjects(origin);
  }

  return {};
}

// Return dormant permission grants for an origin.
std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
ChromeFileSystemAccessPermissionContext::GetDormantPersistedObjects(
    const url::Origin& origin) {
  if (GetPersistedGrantState(origin) == PersistedGrantState::kDormant) {
    return ObjectPermissionContextBase::GetGrantedObjects(origin);
  }
  return {};
}

// Returns extended grants or active grants for an origin.
// TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
// feature flag checks before launch.
std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
ChromeFileSystemAccessPermissionContext::GetGrantedObjects(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OriginHasExtendedPermission(origin)) {
    // When the origin has extended permission enabled, objects stored in
    // content settings map via `ObjectPermissionContextBase` represent a valid
    // set of grants.
    return ObjectPermissionContextBase::GetGrantedObjects(origin);
  }

  // When the extended permission is not enabled, a valid set of grants are
  // stored in the in-memory map |active_permissions_map_|.
  // TODO(crbug.com/1466929): Update iteration logic below to handle the case of
  // write-only permission grants.
  std::vector<std::unique_ptr<Object>> objects;
  auto it = active_permissions_map_.find(origin);
  if (it != active_permissions_map_.end()) {
    for (const auto& grant : it->second.read_grants) {
      if (grant.second->GetStatus() == PermissionStatus::GRANTED) {
        auto value = grant.second->AsValue();
        if (base::Contains(it->second.write_grants, grant.first)) {
          // Persisted permissions include both read and write information in
          // one object. If a write grant for this origin/path exists, then
          // update the value to store a writable key as well.
          value.Set(kPermissionWritableKey, true);
        }
        objects.push_back(std::make_unique<Object>(
            origin, base::Value(std::move(value)),
            content_settings::SettingSource::SETTING_SOURCE_USER,
            IsOffTheRecord()));
      }
    }
  }

  return objects;
}

// Returns all origins' extended grants or active grants.
std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
ChromeFileSystemAccessPermissionContext::GetAllGrantedObjects() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<Object>> all_objects;
  for (const auto& origin : GetOriginsWithGrants()) {
    auto objects = GetGrantedObjects(origin);
    base::ranges::move(objects, std::back_inserter(all_objects));
  }

  return all_objects;
}

// Returns origins that have either extended grants or active grants.
// TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
// feature flag checks before launch.
std::set<url::Origin>
ChromeFileSystemAccessPermissionContext::GetOriginsWithGrants() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<url::Origin> origins;

  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    // Get origins with extended permissions.
    for (const url::Origin& origin :
         ObjectPermissionContextBase::GetOriginsWithGrants()) {
      if (OriginHasExtendedPermission(origin)) {
        origins.insert(origin);
      }
    }
  }

  // Add origins that have active grants.
  for (const auto& it : active_permissions_map_) {
    origins.insert(it.first);
  }

  return origins;
}

std::string ChromeFileSystemAccessPermissionContext::GetKeyForObject(
    const base::Value::Dict& object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto optional_path =
      base::ValueToFilePath(object.Find(kPermissionPathKey));
  DCHECK(optional_path);
  return std::string(PathAsPermissionKey(optional_path.value()));
}

bool ChromeFileSystemAccessPermissionContext::IsValidObject(
    const base::Value::Dict& dict) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (dict.size() != 3 && dict.size() != 4) {
    return false;
  }

  // At least one of the readable/writable keys needs to be set.
  if (!dict.FindBool(kPermissionWritableKey) &&
      !dict.FindBool(kPermissionReadableKey)) {
    return false;
  }

  if (!dict.contains(kPermissionPathKey) ||
      !dict.FindBool(kPermissionIsDirectoryKey) ||
      dict.contains(kDeprecatedPermissionLastUsedTimeKey)) {
    return false;
  }

  return true;
}

std::u16string ChromeFileSystemAccessPermissionContext::GetObjectDisplayName(
    const base::Value::Dict& object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto optional_path =
      base::ValueToFilePath(object.Find(kPermissionPathKey));
  DCHECK(optional_path);
  return optional_path->LossyDisplayName();
}

ContentSetting
ChromeFileSystemAccessPermissionContext::GetWriteGuardContentSetting(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_settings_->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
}

ContentSetting
ChromeFileSystemAccessPermissionContext::GetReadGuardContentSetting(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_settings_->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_READ_GUARD);
}

bool ChromeFileSystemAccessPermissionContext::CanObtainReadPermission(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetReadGuardContentSetting(origin) == CONTENT_SETTING_ASK ||
         GetReadGuardContentSetting(origin) == CONTENT_SETTING_ALLOW;
}

bool ChromeFileSystemAccessPermissionContext::CanObtainWritePermission(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetWriteGuardContentSetting(origin) == CONTENT_SETTING_ASK ||
         GetWriteGuardContentSetting(origin) == CONTENT_SETTING_ALLOW;
}

void ChromeFileSystemAccessPermissionContext::ConfirmSensitiveEntryAccess(
    const url::Origin& origin,
    PathType path_type,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action,
    content::GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto after_blocklist_check_callback = base::BindOnce(
      &ChromeFileSystemAccessPermissionContext::DidCheckPathAgainstBlocklist,
      GetWeakPtr(), origin, path, handle_type, user_action, frame_id,
      std::move(callback));
  CheckPathAgainstBlocklist(path_type, path, handle_type,
                            std::move(after_blocklist_check_callback));
}

void ChromeFileSystemAccessPermissionContext::CheckPathAgainstBlocklist(
    PathType path_type,
    const base::FilePath& path,
    HandleType handle_type,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/1009970): Figure out what external paths should be
  // blocked. We could resolve the external path to a local path, and check for
  // blocked directories based on that, but that doesn't work well. Instead we
  // should have a separate Chrome OS only code path to block for example the
  // root of certain external file systems.
  if (path_type == PathType::kExternal) {
    std::move(callback).Run(/*should_block=*/false);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ShouldBlockAccessToPath, path, handle_type),
      std::move(callback));
}

void ChromeFileSystemAccessPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<content::FileSystemAccessWriteItem> item,
    content::GlobalRenderFrameHostId frame_id,
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
              base::SequencedTaskRunner::GetCurrentDefault(),
              std::move(callback))));
}

void ChromeFileSystemAccessPermissionContext::DidCheckPathAgainstBlocklist(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action,
    content::GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(SensitiveEntryResult)> callback,
    bool should_block) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (user_action == UserAction::kNone) {
    std::move(callback).Run(should_block ? SensitiveEntryResult::kAbort
                                         : SensitiveEntryResult::kAllowed);
    return;
  }

  if (should_block) {
    auto result_callback =
        base::BindPostTaskToCurrentDefault(std::move(callback));
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ShowFileSystemAccessRestrictedDirectoryDialogOnUIThread,
                       frame_id, origin, handle_type,
                       std::move(result_callback)));
    return;
  }

  // If attempting to save a file with a dangerous extension, prompt the user
  // to make them confirm they actually want to save the file.
  if (handle_type == HandleType::kFile && user_action == UserAction::kSave &&
      FileHasDangerousExtension(origin, path,
                                Profile::FromBrowserContext(profile_))) {
    auto result_callback =
        base::BindPostTaskToCurrentDefault(std::move(callback));
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ShowFileSystemAccessDangerousFileDialogOnUIThread,
                       frame_id, origin, path, std::move(result_callback)));
    return;
  }

  std::move(callback).Run(SensitiveEntryResult::kAllowed);
}

void ChromeFileSystemAccessPermissionContext::MaybeEvictEntries(
    base::Value::Dict& dict) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::pair<base::Time, std::string>> entries;
  entries.reserve(dict.size());
  for (auto entry : dict) {
    // Don't evict the default ID.
    if (entry.first == kDefaultLastPickedDirectoryKey) {
      continue;
    }
    // If the data is corrupted and `entry.second` is for some reason not a
    // dict, it should be first in line for eviction.
    auto timestamp = base::Time::Min();
    if (entry.second.is_dict()) {
      timestamp = base::ValueToTime(entry.second.GetDict().Find(kTimestampKey))
                      .value_or(base::Time::Min());
    }
    entries.emplace_back(timestamp, entry.first);
  }

  if (entries.size() <= max_ids_per_origin_) {
    return;
  }

  base::ranges::sort(entries);
  size_t entries_to_remove = entries.size() - max_ids_per_origin_;
  for (size_t i = 0; i < entries_to_remove; ++i) {
    bool did_remove_entry = dict.Remove(entries[i].second);
    DCHECK(did_remove_entry);
  }
}

void ChromeFileSystemAccessPermissionContext::SetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id,
    const base::FilePath& path,
    const PathType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value value = content_settings()->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
      /*info=*/nullptr);
  if (!value.is_dict()) {
    value = base::Value(base::Value::Type::DICT);
  }

  base::Value::Dict& dict = value.GetDict();
  // Create an entry into the nested dictionary.
  base::Value::Dict entry;
  entry.Set(kPathKey, base::FilePathToValue(path));
  entry.Set(kPathTypeKey, static_cast<int>(type));
  entry.Set(kTimestampKey, base::TimeToValue(clock_->Now()));

  dict.Set(GenerateLastPickedDirectoryKey(id), std::move(entry));

  MaybeEvictEntries(dict);

  content_settings_->SetWebsiteSettingDefaultScope(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
      base::Value(std::move(dict)));
}

ChromeFileSystemAccessPermissionContext::PathInfo
ChromeFileSystemAccessPermissionContext::GetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value value = content_settings()->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
      /*info=*/nullptr);

  PathInfo path_info;
  if (!value.is_dict()) {
    return path_info;
  }

  auto* entry = value.GetDict().FindDict(GenerateLastPickedDirectoryKey(id));
  if (!entry) {
    return path_info;
  }

  auto type_int =
      entry->FindInt(kPathTypeKey).value_or(static_cast<int>(PathType::kLocal));
  path_info.type = type_int == static_cast<int>(PathType::kExternal)
                       ? PathType::kExternal
                       : PathType::kLocal;
  path_info.path =
      base::ValueToFilePath(entry->Find(kPathKey)).value_or(base::FilePath());
  return path_info;
}

base::FilePath
ChromeFileSystemAccessPermissionContext::GetWellKnownDirectoryPath(
    blink::mojom::WellKnownDirectory directory,
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // PDF viewer uses the default Download directory set in browser, if possible.
  if (directory == blink::mojom::WellKnownDirectory::kDirDownloads &&
      IsPdfExtensionOrigin(origin)) {
    base::FilePath profile_download_path =
        DownloadPrefs::FromBrowserContext(profile())->DownloadPath();
    if (!profile_download_path.empty()) {
      return profile_download_path;
    }
  }

  int key = base::PATH_START;
  switch (directory) {
    case blink::mojom::WellKnownDirectory::kDirDesktop:
      key = base::DIR_USER_DESKTOP;
      break;
    case blink::mojom::WellKnownDirectory::kDirDocuments:
      key = chrome::DIR_USER_DOCUMENTS;
      break;
    case blink::mojom::WellKnownDirectory::kDirDownloads:
      key = chrome::DIR_DEFAULT_DOWNLOADS;
      break;
    case blink::mojom::WellKnownDirectory::kDirMusic:
      key = chrome::DIR_USER_MUSIC;
      break;
    case blink::mojom::WellKnownDirectory::kDirPictures:
      key = chrome::DIR_USER_PICTURES;
      break;
    case blink::mojom::WellKnownDirectory::kDirVideos:
      key = chrome::DIR_USER_VIDEOS;
      break;
  }
  base::FilePath directory_path;
  base::PathService::Get(key, &directory_path);
  return directory_path;
}

std::u16string ChromeFileSystemAccessPermissionContext::GetPickerTitle(
    const blink::mojom::FilePickerOptionsPtr& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(asully): Consider adding custom strings for invocations of the file
  // picker, as well. Returning the empty string will fall back to the platform
  // default for the given picker type.
  std::u16string title;
  if (options->type_specific_options->is_directory_picker_options()) {
    title = l10n_util::GetStringUTF16(
        options->type_specific_options->get_directory_picker_options()
                ->request_writable
            ? IDS_FILE_SYSTEM_ACCESS_CHOOSER_OPEN_WRITABLE_DIRECTORY_TITLE
            : IDS_FILE_SYSTEM_ACCESS_CHOOSER_OPEN_READABLE_DIRECTORY_TITLE);
  }
  return title;
}

// TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
// feature flag checks before launch.
void ChromeFileSystemAccessPermissionContext::NotifyEntryMoved(
    const url::Origin& origin,
    const base::FilePath& old_path,
    const base::FilePath& new_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (old_path == new_path) {
    return;
  }

  bool updated = false;
  auto it = active_permissions_map_.find(origin);
  if (it != active_permissions_map_.end()) {
    // TODO(https://crbug.com/1381302): Consolidate superfluous child grants.
    PermissionGrantImpl::UpdateGrantPath(it->second.write_grants, old_path,
                                         new_path);
    PermissionGrantImpl::UpdateGrantPath(it->second.read_grants, old_path,
                                         new_path);
    updated = true;
  }

  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    // Active grants are a subset of dormant/extended grants, so we also
    // need to update dormant/extended grants, in case it's not covered by
    // UpdateGrantPath() above.
    const std::unique_ptr<Object> object =
        GetGrantedObject(origin, PathAsPermissionKey(old_path));
    if (object) {
      base::Value::Dict new_object = object->value.Clone();
      new_object.Set(kPermissionPathKey, base::FilePathToValue(new_path));
      UpdateObjectPermission(origin, object->value, std::move(new_object));
      updated = true;
    }
  }

  if (updated) {
    ScheduleUsageIconUpdate();
  }
}

ChromeFileSystemAccessPermissionContext::Grants
ChromeFileSystemAccessPermissionContext::ConvertObjectsToGrants(
    const std::vector<std::unique_ptr<Object>> objects) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ChromeFileSystemAccessPermissionContext::Grants grants;

  for (const auto& object : objects) {
    if (!IsValidObject(object->value)) {
      continue;
    }

    const base::Value::Dict& object_dict = object->value;
    const base::FilePath path =
        base::ValueToFilePath(object_dict.Find(kPermissionPathKey)).value();
    HandleType handle_type =
        object_dict.FindBool(kPermissionIsDirectoryKey).value()
            ? HandleType::kDirectory
            : HandleType::kFile;
    bool is_write_grant =
        object_dict.FindBool(kPermissionWritableKey).value_or(false);
    bool is_read_grant =
        object_dict.FindBool(kPermissionReadableKey).value_or(false);

    if (handle_type == HandleType::kDirectory) {
      if (is_write_grant &&
          !base::Contains(grants.directory_write_grants, path)) {
        grants.directory_write_grants.push_back(path);
      }
      if (is_read_grant &&
          !base::Contains(grants.directory_read_grants, path)) {
        grants.directory_read_grants.push_back(path);
      }
    }
    if (handle_type == HandleType::kFile) {
      if (is_write_grant && !base::Contains(grants.file_write_grants, path)) {
        grants.file_write_grants.push_back(path);
      }
      if (is_read_grant && !base::Contains(grants.file_read_grants, path)) {
        grants.file_read_grants.push_back(path);
      }
    }
  }

  return grants;
}

// TODO(https://crbug.com/1011533): Integrate with Safety Hub for site
// inactivity revocation.
// TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
// feature flag checks before launch.
void ChromeFileSystemAccessPermissionContext::RevokeGrants(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool grant_revoked = false;

  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    std::vector<std::unique_ptr<Object>> extended_or_dormant_objects =
        ObjectPermissionContextBase::GetGrantedObjects(origin);
    for (auto& object : extended_or_dormant_objects) {
      RevokeObjectPermission(origin, GetKeyForObject(object->value));
      grant_revoked = true;
    }
    // TODO(https://crbug.com/1011533): Clear Extended Permission state.
  }

  if (RevokeActiveGrants(origin)) {
    grant_revoked = true;
  }

  if (grant_revoked) {
    ScheduleUsageIconUpdate();
  }
}

// TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
// feature flag checks before launch.
void ChromeFileSystemAccessPermissionContext::RevokeGrant(
    const url::Origin& origin,
    const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool grant_revoked = false;

  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    auto key = PathAsPermissionKey(file_path);
    const std::unique_ptr<Object> object = GetGrantedObject(origin, key);
    if (object) {
      RevokeObjectPermission(origin, key);
      grant_revoked = true;
    }
  }

  if (RevokeActiveGrants(origin, file_path)) {
    grant_revoked = true;
  }

  if (grant_revoked) {
    ScheduleUsageIconUpdate();
  }
}

// TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
// feature flag checks before launch.
bool ChromeFileSystemAccessPermissionContext::OriginHasReadAccess(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First, check if an origin has read access granted via active permissions.
  auto it = active_permissions_map_.find(origin);
  if (it != active_permissions_map_.end()) {
    return base::ranges::any_of(it->second.read_grants, [&](const auto& grant) {
      return grant.second->GetStatus() == PermissionStatus::GRANTED;
    });
  }

  // Check if an origin has read access granted via extended permissions.
  std::vector<std::unique_ptr<Object>> extended_grant_objects =
      GetExtendedPersistedObjects(origin);
  if (extended_grant_objects.empty()) {
    return false;
  }
  return base::ranges::any_of(extended_grant_objects, [&](const auto& grant) {
    return grant->value.FindBool(kPermissionReadableKey).value_or(false);
  });
}

// TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
// feature flag checks before launch.
bool ChromeFileSystemAccessPermissionContext::OriginHasWriteAccess(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First, check if an origin has write access granted via active permissions.
  auto it = active_permissions_map_.find(origin);
  if (it != active_permissions_map_.end()) {
    return base::ranges::any_of(
        it->second.write_grants, [&](const auto& grant) {
          return grant.second->GetStatus() == PermissionStatus::GRANTED;
        });
  }

  // Check if an origin has write access granted via extended permissions.
  std::vector<std::unique_ptr<Object>> extended_grant_objects =
      GetExtendedPersistedObjects(origin);
  if (extended_grant_objects.empty()) {
    return false;
  }
  return base::ranges::any_of(extended_grant_objects, [&](const auto& grant) {
    return grant->value.FindBool(kPermissionWritableKey).value_or(false);
  });
}

void ChromeFileSystemAccessPermissionContext::NavigatedAwayFromOrigin(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = active_permissions_map_.find(origin);
  // If we have no permissions for the origin, there is nothing to do.
  if (it == active_permissions_map_.end()) {
    return;
  }

  // Start a timer to possibly clean up permissions for this origin.
  if (!it->second.cleanup_timer) {
    it->second.cleanup_timer = std::make_unique<base::RetainingOneShotTimer>(
        FROM_HERE, kPermissionRevocationTimeout,
        base::BindRepeating(&ChromeFileSystemAccessPermissionContext::
                                MaybeCleanupActivePermissions,
                            base::Unretained(this), origin));
  }
  it->second.cleanup_timer->Reset();
}

void ChromeFileSystemAccessPermissionContext::TriggerTimersForTesting() {
  for (const auto& it : active_permissions_map_) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (it.second.cleanup_timer) {
      auto task = it.second.cleanup_timer->user_task();
      it.second.cleanup_timer->Stop();
      task.Run();
    }
  }
}

void ChromeFileSystemAccessPermissionContext::MaybeCleanupActivePermissions(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = active_permissions_map_.find(origin);
  // If we have no permissions for the origin, there is nothing to do.
  if (it == active_permissions_map_.end()) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  // Iterate over all top-level frames by iterating over all browsers, and all
  // tabs within those browsers. This also counts PWAs in windows without
  // tab strips, as those are still implemented as a Browser with a single tab.
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile()) {
      continue;
    }
    TabStripModel* tabs = browser->tab_strip_model();
    for (int i = 0; i < tabs->count(); ++i) {
      content::WebContents* web_contents = tabs->GetWebContentsAt(i);
      url::Origin tab_origin = url::Origin::Create(
          permissions::PermissionUtil::GetLastCommittedOriginAsURL(
              web_contents->GetPrimaryMainFrame()));
      // Found a tab for this origin, so early exit and don't revoke grants.
      if (tab_origin == origin) {
        return;
      }
    }
  }

  // No tabs found with the same origin, so revoke all active permissions for
  // the origin.
  if (RevokeActiveGrants(origin)) {
    ScheduleUsageIconUpdate();
  }
#endif
}

bool ChromeFileSystemAccessPermissionContext::AncestorHasActivePermission(
    const url::Origin& origin,
    const base::FilePath& path,
    GrantType grant_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = active_permissions_map_.find(origin);
  if (it == active_permissions_map_.end()) {
    return false;
  }
  const auto& relevant_grants = grant_type == GrantType::kWrite
                                    ? it->second.write_grants
                                    : it->second.read_grants;
  if (relevant_grants.empty()) {
    return false;
  }

  // Permissions are inherited from the closest ancestor.
  for (base::FilePath parent = path.DirName(); parent != parent.DirName();
       parent = parent.DirName()) {
    auto i = relevant_grants.find(parent);
    if (i != relevant_grants.end() && i->second &&
        i->second->GetStatus() == PermissionStatus::GRANTED) {
      return true;
    }
  }
  return false;
}

// TODO(crbug.com/1373962): Remove `kFileSystemAccessPersistentPermissions`
// feature flag checks before launch.
// TODO(crbug.com/1011533): Add checks for `PersistentPermissionPref` value
// once the ContentSetting is implemented.
bool ChromeFileSystemAccessPermissionContext::OriginHasExtendedPermission(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  // The File System Access API is not supported on Android (see
  // crbug.com/1011535). If this ever changes, we'll need to revist this.
  return false;
#else

  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return false;
  }

  const auto& origin_it = extended_permissions_settings_map_.find(origin);
  if (origin_it != extended_permissions_settings_map_.end() &&
      origin_it->second == ContentSetting::CONTENT_SETTING_ALLOW) {
    return true;
  }

  DCHECK(profile());
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(profile()));
  if (!web_app_provider) {
    return false;
  }

  auto app_id = web_app_provider->registrar_unsafe().FindAppWithUrlInScope(
      origin.GetURL());
  return app_id.has_value() &&
         web_app_provider->registrar_unsafe().IsActivelyInstalled(
             app_id.value());
#endif  // BUILDFLAG(IS_ANDROID)
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::
    GetExtendedReadPermissionGrantForTesting(  // IN-TEST
        const url::Origin& origin,
        const base::FilePath& path,
        HandleType handle_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto grant =
      GetReadPermissionGrant(origin, path, handle_type, UserAction::kOpen);

  static_cast<PermissionGrantImpl*>(grant.get())
      ->SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
  return grant;
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::
    GetExtendedWritePermissionGrantForTesting(  // IN-TEST
        const url::Origin& origin,
        const base::FilePath& path,
        HandleType handle_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto grant =
      GetWritePermissionGrant(origin, path, handle_type, UserAction::kSave);

  static_cast<PermissionGrantImpl*>(grant.get())
      ->SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
  return grant;
}

bool ChromeFileSystemAccessPermissionContext::HasExtendedPermissionForTesting(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    GrantType grant_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return HasExtendedPermission(origin, path, handle_type, grant_type);
}

bool ChromeFileSystemAccessPermissionContext::HasExtendedPermission(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    GrantType grant_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!OriginHasExtendedPermission(origin)) {
    return false;
  }

  // Don't persist permissions when the origin is allowlisted or blocked.
  auto content_setting = GetWriteGuardContentSetting(origin);
  if (content_setting == CONTENT_SETTING_ALLOW ||
      content_setting == CONTENT_SETTING_BLOCK) {
    return false;
  }

  // TODO(https://crbug.com/984772): If a parent directory has an extended
  // permission, we should return true here.

  const std::unique_ptr<Object> object =
      GetGrantedObject(origin, PathAsPermissionKey(path));
  if (!object) {
    return false;
  }

  if (object->value.FindBool(kPermissionIsDirectoryKey).value() !=
      (handle_type == HandleType::kDirectory)) {
    return false;
  }

  if (!object->value.FindBool(GetGrantKeyFromGrantType(grant_type))
           .value_or(false)) {
    return false;
  }

  return true;
}

// Determines if a given origin has active grants.
bool ChromeFileSystemAccessPermissionContext::HasGrantedActiveGrant(
    const url::Origin& origin) const {
  auto origin_it = active_permissions_map_.find(origin);
  if (origin_it == active_permissions_map_.end()) {
    return false;
  }
  const OriginState& origin_state = origin_it->second;
  return (base::ranges::any_of(origin_state.read_grants,
                               [&](const auto& grant) {
                                 return grant.second->GetStatus() ==
                                        PermissionStatus::GRANTED;
                               }) ||

          base::ranges::any_of(origin_state.write_grants,
                               [&](const auto& grant) {
                                 return grant.second->GetStatus() ==
                                        PermissionStatus::GRANTED;
                               })

  );
}

ChromeFileSystemAccessPermissionContext::PersistedGrantState
ChromeFileSystemAccessPermissionContext::GetPersistedGrantState(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (OriginHasExtendedPermission(origin)) {
    return PersistedGrantState::kExtended;
  }

  // TODO(crbug.com/1011533): Add check for the bit that will store whether
  // the persisted grants are not dormant, and are the latest grants from
  // this session (either shadow or extended, once it is implemented.
  if (!HasGrantedActiveGrant(origin)) {
    return PersistedGrantState::kDormant;
  }
  return PersistedGrantState::kShadow;
}

void ChromeFileSystemAccessPermissionContext::PermissionGrantDestroyed(
    PermissionGrantImpl* grant) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = active_permissions_map_.find(grant->origin());
  if (it == active_permissions_map_.end()) {
    return;
  }

  auto& grants = grant->type() == GrantType::kRead ? it->second.read_grants
                                                   : it->second.write_grants;
  auto grant_it = grants.find(grant->GetPath());
  // Any non-denied permission grants should have still been in our grants
  // list. If this invariant is violated we would have permissions that might
  // be granted but won't be visible in any UI because the permission context
  // isn't tracking them anymore.
  if (grant_it == grants.end()) {
    DCHECK_EQ(PermissionStatus::DENIED, grant->GetStatus());
    return;
  }

  // The grant in |grants| for this path might have been replaced with a
  // different grant. Only erase if it actually matches the grant that was
  // destroyed.
  if (grant_it->second == grant) {
    grants.erase(grant_it);
  }

  ScheduleUsageIconUpdate();
}

void ChromeFileSystemAccessPermissionContext::ScheduleUsageIconUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (usage_icon_update_scheduled_) {
    return;
  }
  usage_icon_update_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChromeFileSystemAccessPermissionContext::DoUsageIconUpdate,
          weak_factory_.GetWeakPtr()));
}

void ChromeFileSystemAccessPermissionContext::DoUsageIconUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_icon_update_scheduled_ = false;
#if !BUILDFLAG(IS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile()) {
      continue;
    }
    browser->window()->UpdatePageActionIcon(
        PageActionIconType::kFileSystemAccess);
  }
#endif
}

base::WeakPtr<ChromeFileSystemAccessPermissionContext>
ChromeFileSystemAccessPermissionContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
