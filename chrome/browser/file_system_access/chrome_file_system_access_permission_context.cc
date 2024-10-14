// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
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
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/file_system_access/file_system_access_dangerous_file_dialog.h"
#include "chrome/browser/ui/file_system_access/file_system_access_dialogs.h"
#include "chrome/browser/ui/file_system_access/file_system_access_restricted_directory_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/pdf/common/pdf_util.h"
#include "components/permissions/features.h"
#include "components/permissions/object_permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "extensions/browser/extension_registry.h"  // nogncheck
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_APPS)
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#endif

namespace {

using FileRequestData =
    FileSystemAccessPermissionRequestManager::FileRequestData;
using RequestAccess = FileSystemAccessPermissionRequestManager::Access;
using HandleType = content::FileSystemAccessPermissionContext::HandleType;
using PersistedGrantStatus =
    ChromeFileSystemAccessPermissionContext::PersistedGrantStatus;
using GrantType = ChromeFileSystemAccessPermissionContext::GrantType;
using blink::mojom::PermissionStatus;
using permissions::PermissionAction;

// This long after the last top-level tab or window for an origin is closed (or
// is navigated to another origin), all the permissions for that origin will be
// revoked.
constexpr base::TimeDelta kPermissionRevocationTimeout = base::Seconds(5);

// Dictionary keys for the FILE_SYSTEM_ACCESS_CHOOSER_DATA setting.
// `kPermissionPathKey[] = "path"` is defined in the header file.
const char kPermissionDisplayNameKey[] = "display-name";
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
const char kDisplayNameKey[] = "display-name";
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
    const content::PathInfo& path_info,
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

  ShowFileSystemAccessDangerousFileDialog(origin, path_info,
                                          std::move(callback), web_contents);
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
    {base::DIR_MODULE, nullptr, kBlockAllChildren},
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
    // Block access to the user's Applications directory.
    {base::DIR_HOME, FILE_PATH_LITERAL("Applications"), kBlockAllChildren},
    // Block access to the root Applications directory.
    {kNoBasePathKey, FILE_PATH_LITERAL("/Applications"), kBlockAllChildren},
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_ANDROID)
    {base::DIR_ANDROID_APP_DATA, nullptr, kBlockAllChildren},
    {base::DIR_CACHE, nullptr, kBlockAllChildren},
#endif
    // TODO(crbug.com/40095723): Refine this list, for example add
    // XDG_CONFIG_HOME when it is not set ~/.config?
};

// Describes a rule for blocking a directory, which can be constructed
// dynamically (based on state) or statically (from kBlockedPaths).
struct BlockPathRule {
  base::FilePath path;
  BlockType type;
};

// A wrapper around `base::NormalizeFilePath` that returns its result instead of
// using an out parameter.
base::FilePath NormalizeFilePath(const base::FilePath& path) {
  CHECK(path.IsAbsolute());
  // TODO(crbug.com/368130513O): On Windows, this call will fail if the target
  // file path is greater than MAX_PATH. We should decide how to handle this
  // scenario.
  base::FilePath normalized_path;
  if (!base::NormalizeFilePath(path, &normalized_path)) {
    return path;
  }
  CHECK_EQ(path.empty(), normalized_path.empty());
  return normalized_path;
}

bool ShouldBlockAccessToPath(const base::FilePath& path,
                             HandleType handle_type,
                             std::vector<BlockPathRule> rules) {
  DCHECK(!path.empty());
#if BUILDFLAG(IS_ANDROID)
  // The only check for content-URIs is that they are not from an internal
  // FileProvider.
  if (path.IsContentUri()) {
    base::android::BuildInfo* info = base::android::BuildInfo::GetInstance();
    return base::StartsWith(
        path.value(), base::StrCat({"content://", info->package_name(), "."}),
        base::CompareCase::INSENSITIVE_ASCII);
  }
#endif
  DCHECK(path.IsAbsolute());

  bool normalize_file_paths = base::FeatureList::IsEnabled(
      features::kFileSystemAccessSymbolicLinkCheck);

  base::FilePath check_path =
      normalize_file_paths ? NormalizeFilePath(path) : path;

#if BUILDFLAG(IS_WIN)
  // On Windows, local UNC paths are rejected, as UNC path can be written in a
  // way that can bypass the blocklist.
  if (MaybeIsLocalUNCPath(check_path)) {
    return true;
  }
#endif

  // Add the hard-coded rules to the dynamic rules.
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
    rules.emplace_back(blocked_path, block.type);
  }

  base::FilePath nearest_ancestor;
  BlockType nearest_ancestor_block_type = kDontBlockChildren;
  for (const auto& block : rules) {
    base::FilePath blocked_path =
        normalize_file_paths ? NormalizeFilePath(block.path) : block.path;

    if (check_path == blocked_path || check_path.IsParent(blocked_path)) {
      VLOG(1) << "Blocking access to " << check_path
              << " because it is a parent of " << blocked_path;
      return true;
    }

    if (blocked_path.IsParent(check_path) &&
        (nearest_ancestor.empty() || nearest_ancestor.IsParent(blocked_path))) {
      nearest_ancestor = blocked_path;
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
          << nearest_ancestor;
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
    case Result::DANGEROUS_ACCOUNT_COMPROMISE:
    case Result::BLOCKED_SCAN_FAILED:
      return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::
          kBlock;

    // This shouldn't be returned for File System Access write checks.
    case Result::ASYNC_SCANNING:
    case Result::ASYNC_LOCAL_PASSWORD_SCANNING:
    case Result::SENSITIVE_CONTENT_WARNING:
    case Result::SENSITIVE_CONTENT_BLOCK:
    case Result::DEEP_SCANNED_SAFE:
    case Result::PROMPT_FOR_SCANNING:
    case Result::PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case Result::DEEP_SCANNED_FAILED:
    case Result::IMMEDIATE_DEEP_SCAN:
      NOTREACHED_IN_MIGRATION();
      return ChromeFileSystemAccessPermissionContext::AfterWriteCheckResult::
          kAllow;
  }
  NOTREACHED_IN_MIGRATION();
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

safe_browsing::DownloadFileType::DangerLevel GetFileTypeDangerLevel(
    const base::FilePath& path,
    const url::Origin& origin,
    Profile* profile) {
  return safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
      path, origin.GetURL(), profile->GetPrefs());
}

std::string StringOrEmpty(const std::string* s) {
  return s ? *s : std::string();
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
      const content::PathInfo& path_info,
      HandleType handle_type,
      GrantType type,
      UserAction user_action)
      : context_(std::move(context)),
        origin_(origin),
        handle_type_(handle_type),
        type_(type),
        path_info_(path_info),
        user_action_(user_action) {}

  // FileSystemAccessPermissionGrant:
  PermissionStatus GetStatus() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(crbug.com/40101962): Determine if this should return denied for
    // guard block, and how ancestor permission should be handled.
    if (status_ == PermissionStatus::ASK &&
        context_->CanAutoGrantViaPersistentPermission(origin_, path_info_.path,
                                                      handle_type_, type_)) {
      return PermissionStatus::GRANTED;
    }
    return status_;
  }

  PermissionStatus GetActivePermissionStatus() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return status_;
  }

  base::FilePath GetPath() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_info_.path;
  }

  std::string GetDisplayName() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_info_.display_name;
  }

  void RequestPermission(
      content::GlobalRenderFrameHostId frame_id,
      UserActivationState user_activation_state,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Check if a permission request has already been processed previously. This
    // check is done first because we don't want to reset the status of a
    // permission if it has already been granted.
    if (GetActivePermissionStatus() != PermissionStatus::ASK || !context_) {
      if (GetActivePermissionStatus() == PermissionStatus::GRANTED) {
        SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
      }
      std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
      return;
    }

    if (type_ == GrantType::kWrite) {
      ContentSetting content_setting =
          context_->GetWriteGuardContentSetting(origin_);

      // Content setting grants write permission without asking.
      if (content_setting == CONTENT_SETTING_ALLOW) {
        PermissionRequestOutcome outcome =
            PermissionRequestOutcome::kGrantedByContentSetting;
        RecordPermissionRequestOutcome(outcome);
        // May destroy `this`.
        SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
        std::move(callback).Run(outcome);
        return;
      }

      // Content setting blocks write permission.
      if (content_setting == CONTENT_SETTING_BLOCK) {
        PermissionRequestOutcome outcome =
            PermissionRequestOutcome::kBlockedByContentSetting;
        RecordPermissionRequestOutcome(outcome);
        // May destroy `this`.
        SetStatus(PermissionStatus::DENIED,
                  PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
        std::move(callback).Run(outcome);
        return;
      }
    }

    if (context_->CanAutoGrantViaPersistentPermission(origin_, path_info_.path,
                                                      handle_type_, type_)) {
      PermissionRequestOutcome outcome =
          PermissionRequestOutcome::kGrantedByPersistentPermission;
      RecordPermissionRequestOutcome(outcome);
      // May destroy `this`.
      SetStatus(PermissionStatus::GRANTED,
                PersistedPermissionOptions::kUpdatePersistedPermission);
      std::move(callback).Run(outcome);
      return;
    }

    if (context_->CanAutoGrantViaAncestorPersistentPermission(
            origin_, path_info_.path, type_)) {
      PermissionRequestOutcome outcome =
          PermissionRequestOutcome::kGrantedByAncestorPersistentPermission;
      RecordPermissionRequestOutcome(outcome);
      // May destroy `this`.
      SetStatus(PermissionStatus::GRANTED,
                PersistedPermissionOptions::kUpdatePersistedPermission);
      std::move(callback).Run(outcome);
      return;
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
        web_contents->ForSecurityDropFullscreen(
            /*display_id=*/display::kInvalidDisplayId);

    if (context_->IsEligibleToUpgradePermissionRequestToRestorePrompt(
            origin_, path_info_.path, handle_type_, user_action_, type_)) {
      std::vector<FileRequestData> request_data_list =
          context_->GetFileRequestDataForRestorePermissionPrompt(origin_);
      request_manager->AddRequest(
          {FileSystemAccessPermissionRequestManager::RequestType::
               kRestorePermissions,
           origin_, request_data_list},
          base::BindOnce(&PermissionGrantImpl::OnRestorePermissionRequestResult,
                         this, std::move(callback)),
          std::move(fullscreen_block));
      return;
    }

    // If a website wants both read and write access, code in content will
    // request those as two separate requests. The |request_manager| will then
    // detect this and combine the two requests into one prompt. As such this
    // code does not have to have any way to request Access::kReadWrite.
    FileRequestData file_request_data = {path_info_, handle_type_,
                                         type_ == GrantType::kRead
                                             ? RequestAccess::kRead
                                             : RequestAccess::kWrite};
    request_manager->AddRequest(
        {FileSystemAccessPermissionRequestManager::RequestType::kNewPermission,
         origin_,
         {file_request_data}},
        base::BindOnce(&PermissionGrantImpl::OnPermissionRequestResult, this,
                       std::move(callback)),
        std::move(fullscreen_block));
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

  // `this` may be destroyed. A `FileSystemAccessPermissionGrant::Observer` may
  // destroy `this` when notified of this the status change.
  void SetStatus(PermissionStatus new_status,
                 PersistedPermissionOptions update_options) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto permission_changed = status_ != new_status;
    status_ = new_status;

    if (context_ &&
        update_options ==
            PersistedPermissionOptions::kUpdatePersistedPermission &&
        base::FeatureList::IsEnabled(
            features::kFileSystemAccessPersistentPermissions)) {
      const std::unique_ptr<Object> object = context_->GetGrantedObject(
          origin_, PathAsPermissionKey(path_info_.path));
      auto opposite_type =
          type_ == GrantType::kRead ? GrantType::kWrite : GrantType::kRead;
      if (new_status == PermissionStatus::GRANTED) {
        if (object) {
          // Persisted permissions include both read and write information in
          // one object. Figure out if the other grant type is already
          // persisted and update the existing one.
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
      } else if (object) {
        // Permission is not granted anymore. Remove the grant object entirely
        // if only this grant type exists in the grant object; otherwise, remove
        // the grant type key from the grant object.
        auto type_exists =
            object->value.FindBool(GetGrantKeyFromGrantType(type_))
                .value_or(false);
        auto opposite_type_exists =
            object->value.FindBool(GetGrantKeyFromGrantType(opposite_type))
                .value_or(false);
        if (type_exists) {
          if (opposite_type_exists) {
            base::Value::Dict new_object = object->value.Clone();
            new_object.Remove(GetGrantKeyFromGrantType(type_));
            context_->UpdateObjectPermission(origin_, object->value,
                                             std::move(new_object));
          } else {
            context_->RevokeObjectPermission(origin_, GetKey());
          }
        }
      }
    }

    if (permission_changed) {
      // May destroy `this`.
      NotifyPermissionStatusChanged();
    }
  }

  base::Value::Dict AsValue() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::Value::Dict value;
    value.Set(kPermissionPathKey, base::FilePathToValue(path_info_.path));
    value.Set(kPermissionDisplayNameKey, path_info_.display_name);
    value.Set(kPermissionIsDirectoryKey,
              handle_type_ == HandleType::kDirectory);
    value.Set(GetGrantKeyFromGrantType(type_), true);
    return value;
  }

  static void UpdateGrantPath(
      std::map<base::FilePath, raw_ptr<PermissionGrantImpl, CtnExperimental>>&
          grants,
      const content::PathInfo& old_path,
      const content::PathInfo& new_path) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    auto entry_it =
        base::ranges::find_if(grants, [&old_path](const auto& entry) {
          return entry.first == old_path.path;
        });

    if (entry_it == grants.end()) {
      // There must be an entry for an ancestor of this entry. Nothing to do
      // here.
      //
      // TODO(crbug.com/40245144): Consolidate superfluous child grants
      // to support directory moves.
      return;
    }

    DCHECK_EQ(entry_it->second->GetActivePermissionStatus(),
              PermissionStatus::GRANTED);

    auto* const grant_impl = entry_it->second.get();
    grant_impl->SetPath(new_path);

    // Update the permission grant's key in the map of active permissions.
    grants.erase(entry_it);
    grants.emplace(new_path.path, grant_impl);
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

    if (context_) {
      context_->UpdateGrantsOnPermissionRequestResult(origin_);
    }

    switch (result) {
      case PermissionAction::GRANTED: {
        PermissionRequestOutcome outcome =
            PermissionRequestOutcome::kUserGranted;
        RecordPermissionRequestOutcome(outcome);
        // May destroy `this`.
        SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
        std::move(callback).Run(outcome);
        break;
      }
      case PermissionAction::DENIED: {
        PermissionRequestOutcome outcome =
            PermissionRequestOutcome::kUserDenied;
        RecordPermissionRequestOutcome(outcome);
        // May destroy `this`.
        SetStatus(PermissionStatus::DENIED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
        std::move(callback).Run(outcome);
        break;
      }
      case PermissionAction::DISMISSED:
      case PermissionAction::IGNORED:
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDismissed);
        break;
      case PermissionAction::REVOKED:
      case PermissionAction::GRANTED_ONCE:
      case PermissionAction::NUM:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void OnRestorePermissionRequestResult(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionAction result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!context_) {
      std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
      return;
    }

    // TODO(crbug.com/40101962): Consider adding more `PermissionRequestOutcome`
    // types to account for "restore every time" case and invalid state case.

    if (context_->GetPersistedGrantType(origin_) !=
        PersistedGrantType::kDormant) {
      // User may have enabled the extended permission, or the persisted grant
      // status is changed while the prompt is shown.
      std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
      return;
    }

    switch (result) {
      case PermissionAction::GRANTED:
        context_->OnRestorePermissionAllowedEveryTime(origin_);
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
            RestorePermissionPromptOutcome::kAllowed);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback),
            PermissionRequestOutcome::kGrantedByRestorePrompt);
        break;
      case PermissionAction::GRANTED_ONCE:
        context_->OnRestorePermissionAllowedOnce(origin_);
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
            RestorePermissionPromptOutcome::kAllowedOnce);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback),
            PermissionRequestOutcome::kGrantedByRestorePrompt);
        break;
      case PermissionAction::DENIED:
        context_->OnRestorePermissionDeniedOrDismissed(origin_);
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
            RestorePermissionPromptOutcome::kRejected);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDenied);
        break;
      case PermissionAction::DISMISSED:
        context_->OnRestorePermissionDeniedOrDismissed(origin_);
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
            RestorePermissionPromptOutcome::kDismissed);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDismissed);
        break;
      case PermissionAction::IGNORED:
        // TODO(crbug.com/40101962): This action is not user-detectable,
        // consider replacing `PermissionRequestOutcome` with a more
        // appropriate type.
        context_->OnRestorePermissionIgnored(origin_);
        base::UmaHistogramEnumeration(
            "Storage.FileSystemAccess.RestorePermissionPromptOutcome",
            RestorePermissionPromptOutcome::kIgnored);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kRequestAborted);
        break;
      case PermissionAction::REVOKED:
      case PermissionAction::NUM:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void RecordPermissionRequestOutcome(PermissionRequestOutcome outcome) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (context_ &&
        (outcome ==
             PermissionRequestOutcome::kGrantedByAncestorPersistentPermission ||
         outcome == PermissionRequestOutcome::kGrantedByPersistentPermission ||
         outcome == PermissionRequestOutcome::kGrantedByRestorePrompt)) {
      context_->ScheduleUsageIconUpdate();
    }
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
  }

  void RunCallbackAndRecordPermissionRequestOutcome(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionRequestOutcome outcome) {
    RecordPermissionRequestOutcome(outcome);

    std::move(callback).Run(outcome);
  }

  std::string_view GetKey() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return PathAsPermissionKey(path_info_.path);
  }

  void SetPath(const content::PathInfo& new_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (path_info_ == new_path) {
      return;
    }

    path_info_ = new_path;

    if (base::FeatureList::IsEnabled(
            features::kFileSystemAccessPersistentPermissions)) {
      const std::unique_ptr<Object> object = context_->GetGrantedObject(
          origin_, PathAsPermissionKey(path_info_.path));
      if (object) {
        base::Value::Dict new_object = object->value.Clone();
        new_object.Set(kPermissionPathKey,
                       base::FilePathToValue(new_path.path));
        new_object.Set(kPermissionDisplayNameKey, new_path.display_name);
        context_->UpdateObjectPermission(origin_, object->value,
                                         std::move(new_object));
      }
    }

    // May destroy `this`.
    NotifyPermissionStatusChanged();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<ChromeFileSystemAccessPermissionContext> const context_;
  const url::Origin origin_;
  const HandleType handle_type_;
  const GrantType type_;
  // `path_info_.path` can be updated if the entry is moved.
  content::PathInfo path_info_;
  const UserAction user_action_;

  // This member should only be updated via SetStatus(), to make sure
  // observers are properly notified about any change in status.
  PermissionStatus status_ = PermissionStatus::ASK;
};

struct ChromeFileSystemAccessPermissionContext::OriginState {
  // Raw pointers, owned collectively by all the handles that reference this
  // grant. When last reference goes away this state is cleared as well by
  // PermissionGrantDestroyed().
  std::map<base::FilePath, raw_ptr<PermissionGrantImpl, CtnExperimental>>
      read_grants;
  std::map<base::FilePath, raw_ptr<PermissionGrantImpl, CtnExperimental>>
      write_grants;

  PersistedGrantStatus persisted_grant_status = PersistedGrantStatus::kLoaded;

  // Cached data about whether this origin has an actively installed web app.
  // This is used to determine the origin's extended permission eligibility.
  WebAppInstallStatus web_app_install_status = WebAppInstallStatus::kUnknown;

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

#if BUILDFLAG(IS_ANDROID)
  one_time_permissions_tracker_.Observe(
      OneTimePermissionsTrackerFactory::GetForBrowserContext(context));
#else
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(profile_));
  if (provider) {
    install_manager_observation_.Observe(&provider->install_manager());
  }
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    one_time_permissions_tracker_.Observe(
        OneTimePermissionsTrackerFactory::GetForBrowserContext(context));

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
#endif
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
    for (auto grant_iter = origin_state.read_grants.begin(),
              grant_end = origin_state.read_grants.end();
         grant_iter != grant_end;) {
      // The grant may be removed from `read_grants`, so increase the iterator
      // before continuing.
      auto& grant = *(grant_iter++);
      if (file_path.empty() || grant.first == file_path) {
        if (grant.second) {
          grant.second->SetStatus(
              PermissionStatus::ASK,
              PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
          grant_revoked = true;
        }
      }
    }
    for (auto grant_iter = origin_state.write_grants.begin(),
              grant_end = origin_state.write_grants.end();
         grant_iter != grant_end;) {
      // The grant may be removed from `write_grants`, so increase the iterator
      // before continuing.
      auto& grant = *(grant_iter++);
      if (file_path.empty() || grant.first == file_path) {
        if (grant.second) {
          grant.second->SetStatus(
              PermissionStatus::ASK,
              PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
          grant_revoked = true;
        }
      }
    }
    // Only update `persisted_grant_status` if the state has not already been
    // set via tab backgrounding.
    if (file_path.empty() && origin_state.persisted_grant_status !=
                                 PersistedGrantStatus::kBackgrounded) {
      origin_state.persisted_grant_status = PersistedGrantStatus::kLoaded;
    }
  }
  return grant_revoked;
}

void ChromeFileSystemAccessPermissionContext::RevokeAllActiveGrants() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& [origin, origin_state] : active_permissions_map_) {
    // Only update `persisted_grant_status` if the state has not already been
    // set via tab backgrounding. We do this before iterating over grants so
    // `FileSystemAccessPermissionGrant::Observer`s can update their state
    // correctly.
    if (origin_state.persisted_grant_status !=
        PersistedGrantStatus::kBackgrounded) {
      origin_state.persisted_grant_status = PersistedGrantStatus::kLoaded;
    }

    for (auto grant_iter = origin_state.read_grants.begin(),
              grant_end = origin_state.read_grants.end();
         grant_iter != grant_end;) {
      // The grant may be removed from `read_grants`, so increase the iterator
      // before continuing.
      auto& [_, grant] = *(grant_iter++);
      grant->SetStatus(
          PermissionStatus::ASK,
          PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
    }
    for (auto grant_iter = origin_state.write_grants.begin(),
              grant_end = origin_state.write_grants.end();
         grant_iter != grant_end;) {
      // The grant may be removed from `write_grants`, so increase the iterator
      // before continuing.
      auto& [_, grant] = *(grant_iter++);
      grant->SetStatus(
          PermissionStatus::ASK,
          PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
    }
  }
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const content::PathInfo& path_info,
    HandleType handle_type,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |active_permissions_map_|,
  // but that is exactly what we want.
  auto& origin_state = active_permissions_map_[origin];
  auto& existing_grant = origin_state.read_grants[path_info.path];
  scoped_refptr<PermissionGrantImpl> grant;

  if (existing_grant && existing_grant->handle_type() != handle_type) {
    // |path| changed from being a directory to being a file or vice versa,
    // don't just re-use the existing grant but revoke the old grant before
    // creating a new grant.
    existing_grant->SetStatus(
        PermissionStatus::DENIED,
        PersistedPermissionOptions::kUpdatePersistedPermission);
    existing_grant = nullptr;
  }

  bool creating_new_grant = !existing_grant;
  if (creating_new_grant) {
    grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path_info, handle_type,
        GrantType::kRead, user_action);
    existing_grant = grant.get();
  } else {
    grant = existing_grant;
  }

  const ContentSetting content_setting = GetReadGuardContentSetting(origin);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      // Don't persist permissions when the origin is allowlisted.
      grant->SetStatus(
          PermissionStatus::GRANTED,
          PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      break;
    case CONTENT_SETTING_ASK:
      // If a parent directory is already readable this new grant should also be
      // readable.
      if (creating_new_grant && AncestorHasActivePermission(
                                    origin, path_info.path, GrantType::kRead)) {
        grant->SetStatus(
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
          grant->SetStatus(
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
      if (creating_new_grant) {
        grant->SetStatus(
            PermissionStatus::DENIED,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      } else {
        // We won't revoke permission to an existing grant.
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (HasGrantedActivePermissionStatus(grant.get())) {
    ScheduleUsageIconUpdate();
  }

  return grant;
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const content::PathInfo& path_info,
    HandleType handle_type,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |active_permissions_map_|,
  // but that is exactly what we want.
  auto& origin_state = active_permissions_map_[origin];
  auto& existing_grant = origin_state.write_grants[path_info.path];
  scoped_refptr<PermissionGrantImpl> grant;

  if (existing_grant && existing_grant->handle_type() != handle_type) {
    // |path| changed from being a directory to being a file or vice versa,
    // don't just re-use the existing grant but revoke the old grant before
    // creating a new grant.
    existing_grant->SetStatus(
        PermissionStatus::DENIED,
        PersistedPermissionOptions::kUpdatePersistedPermission);
    existing_grant = nullptr;
  }

  bool creating_new_grant = !existing_grant;
  if (creating_new_grant) {
    grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path_info, handle_type,
        GrantType::kWrite, user_action);
    existing_grant = grant.get();
  } else {
    grant = existing_grant;
  }

  const ContentSetting content_setting = GetWriteGuardContentSetting(origin);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      // Don't persist permissions when the origin is allowlisted.
      grant->SetStatus(
          PermissionStatus::GRANTED,
          PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      break;
    case CONTENT_SETTING_ASK:
      // If a parent directory is already writable this new grant should also be
      // writable.
      if (creating_new_grant &&
          AncestorHasActivePermission(origin, path_info.path,
                                      GrantType::kWrite)) {
        grant->SetStatus(
            PermissionStatus::GRANTED,
            PersistedPermissionOptions::kUpdatePersistedPermission);
        break;
      }
      switch (user_action) {
        case UserAction::kSave:
          // Only automatically grant write access for save dialogs.
          grant->SetStatus(
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
      if (creating_new_grant) {
        grant->SetStatus(
            PermissionStatus::DENIED,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      } else {
        // We won't revoke permission to an existing grant.
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (HasGrantedActivePermissionStatus(grant.get())) {
    ScheduleUsageIconUpdate();
  }

  return grant;
}

// Return extended permission grants for an origin.
std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
ChromeFileSystemAccessPermissionContext::GetExtendedPersistedObjects(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetPersistedGrantType(origin) == PersistedGrantType::kExtended) {
    // When the origin has extended permission enabled, all permissions objects
    // represent extended grants.
    return ObjectPermissionContextBase::GetGrantedObjects(origin);
  }

  return {};
}

// Returns extended grants or active grants for an origin.
std::vector<std::unique_ptr<permissions::ObjectPermissionContextBase::Object>>
ChromeFileSystemAccessPermissionContext::GetGrantedObjects(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto persisted_grant_type = GetPersistedGrantType(origin);
  if (persisted_grant_type == PersistedGrantType::kExtended ||
      persisted_grant_type == PersistedGrantType::kShadow) {
    // Objects stored in content settings map via `ObjectPermissionContextBase`
    // represent a valid set of grants, if origin has extended or shadow grants.
    // In the case of shadow grants, it should be matching the set of active
    // permission map, but it may not have `PermissionStatus::GRANTED`, if the
    // page is refreshed and `FileSystemAccessPermissionGrant` is
    // garbage-collected, hence returning shadow grant objects directly here.
    return ObjectPermissionContextBase::GetGrantedObjects(origin);
  }

  // Otherwise, a valid set of grants are stored in the in-memory map
  // |active_permissions_map_|.
  // TODO(crbug.com/40276567): Update iteration logic below to handle the case
  // of write-only permission grants.
  std::vector<std::unique_ptr<Object>> objects;
  auto it = active_permissions_map_.find(origin);
  if (it != active_permissions_map_.end()) {
    for (const auto& grant : it->second.read_grants) {
      if (HasGrantedActivePermissionStatus(grant.second)) {
        auto value = grant.second->AsValue();

        // Persisted permissions include both read and write information in
        // one object. If a write grant for this origin/path exists, then
        // update the value to store a writable key as well.
        auto file_path = grant.first;
        auto write_grant_it = it->second.write_grants.find(file_path);
        if (write_grant_it != it->second.write_grants.end() &&
            HasGrantedActivePermissionStatus(write_grant_it->second)) {
          value.Set(kPermissionWritableKey, true);
        }

        objects.push_back(std::make_unique<Object>(
            origin, base::Value(std::move(value)),
            content_settings::SettingSource::kUser, IsOffTheRecord()));
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

  // Add origins that have active, granted permission grants.
  for (const auto& it : active_permissions_map_) {
    if (base::ranges::any_of(it.second.read_grants,
                             [&](const auto& grant) {
                               return HasGrantedActivePermissionStatus(
                                   grant.second);
                             }) ||
        base::ranges::any_of(it.second.write_grants, [&](const auto& grant) {
          return HasGrantedActivePermissionStatus(grant.second);
        })) {
      origins.insert(it.first);
    }
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

  if (dict.size() < 3 || dict.size() > 5) {
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
ChromeFileSystemAccessPermissionContext::GetReadGuardContentSetting(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_settings_->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_READ_GUARD);
}

ContentSetting
ChromeFileSystemAccessPermissionContext::GetWriteGuardContentSetting(
    const url::Origin& origin) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content_settings_->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);
}

std::vector<base::FilePath>
ChromeFileSystemAccessPermissionContext::GetGrantedPaths(
    const url::Origin& origin) {
  std::vector<base::FilePath> granted_paths;
  auto granted_objects = GetGrantedObjects(origin);
  for (auto& granted_object : granted_objects) {
    auto* const optional_path = granted_object->value.Find(kPermissionPathKey);
    DCHECK(optional_path);
    granted_paths.push_back(base::ValueToFilePath(optional_path).value());
  }
  return granted_paths;
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

bool ChromeFileSystemAccessPermissionContext::IsFileTypeDangerous(
    const base::FilePath& path,
    const url::Origin& origin) {
  return GetFileTypeDangerLevel(path, origin,
                                Profile::FromBrowserContext(profile_)) ==
         safe_browsing::DownloadFileType::DANGEROUS;
}

void ChromeFileSystemAccessPermissionContext::ConfirmSensitiveEntryAccess(
    const url::Origin& origin,
    const content::PathInfo& path_info,
    HandleType handle_type,
    UserAction user_action,
    content::GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto after_blocklist_check_callback = base::BindOnce(
      &ChromeFileSystemAccessPermissionContext::DidCheckPathAgainstBlocklist,
      GetWeakPtr(), origin, path_info, handle_type, user_action, frame_id,
      std::move(callback));
  CheckPathAgainstBlocklist(path_info, handle_type,
                            std::move(after_blocklist_check_callback));
}

void ChromeFileSystemAccessPermissionContext::CheckPathsAgainstEnterprisePolicy(
    std::vector<content::PathInfo> entries,
    content::GlobalRenderFrameHostId frame_id,
    EntriesAllowedByEnterprisePolicyCallback callback) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  // Get WebContents pointer in order to perform enterprise content analysis.
  content::WebContents* web_contents = nullptr;
  if (!entries.empty()) {
    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
    if (rfh && rfh->IsActive()) {
      web_contents = content::WebContents::FromRenderFrameHost(rfh);
    }
  }

  if (!web_contents) {
    std::move(callback).Run(std::move(entries));
    return;
  }

  enterprise_connectors::ContentAnalysisDelegate::Data data;
  if (!enterprise_connectors::ContentAnalysisDelegate::IsEnabled(
          Profile::FromBrowserContext(profile()),
          web_contents->GetLastCommittedURL(), &data,
          enterprise_connectors::AnalysisConnector::FILE_ATTACHED)) {
    std::move(callback).Run(std::move(entries));
    return;
  }

  data.reason =
      enterprise_connectors::ContentAnalysisRequest::FILE_PICKER_DIALOG;

  // Move the paths from `entries` to `data.paths` to minimize memory copies.
  // Later the paths will be recombined with the type left in `entries` for
  // those files that pass enterprise policy checks.
  std::transform(
      std::make_move_iterator(entries.begin()),
      std::make_move_iterator(entries.end()), std::back_inserter(data.paths),
      [](content::PathInfo&& entry) { return std::move(entry.path); });

  // TODO: crbug.com/326618625 - Handle kExternal files correctly.
  // CreateForFilesInWebContents() only handles real OS files, so these entries
  // are ignored and passed directly to OnContentAnalysisComplete() unchanged.
  // kExternal files only exist in ChromeOS.
  enterprise_connectors::ContentAnalysisDelegate::CreateForFilesInWebContents(
      web_contents, std::move(data),
      base::BindOnce(
          &ChromeFileSystemAccessPermissionContext::OnContentAnalysisComplete,
          weak_factory_.GetWeakPtr(), std::move(entries), std::move(callback)),
      safe_browsing::DeepScanAccessPoint::UPLOAD);
#else
  std::move(callback).Run(std::move(entries));
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

void ChromeFileSystemAccessPermissionContext::OnContentAnalysisComplete(
    std::vector<content::PathInfo> entries,
    EntriesAllowedByEnterprisePolicyCallback callback,
    std::vector<base::FilePath> paths,
    std::vector<bool> allowed) {
  CHECK_EQ(paths.size(), allowed.size());
  CHECK_EQ(paths.size(), entries.size());

  std::vector<content::PathInfo> result_entries;
  for (size_t i = 0; i < paths.size(); ++i) {
    if (allowed[i]) {
      result_entries.emplace_back(entries[i].type, std::move(paths[i]),
                                  entries[i].display_name);
    }
  }

  std::move(callback).Run(std::move(result_entries));
}

#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

void ChromeFileSystemAccessPermissionContext::CheckPathAgainstBlocklist(
    const content::PathInfo& path_info,
    HandleType handle_type,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40101272): Figure out what external paths should be
  // blocked. We could resolve the external path to a local path, and check for
  // blocked directories based on that, but that doesn't work well. Instead we
  // should have a separate Chrome OS only code path to block for example the
  // root of certain external file systems.
  if (path_info.type == content::PathType::kExternal) {
    std::move(callback).Run(/*should_block=*/false);
    return;
  }

  // Unlike the DIR_USER_DATA check, this handles the --user-data-dir override.
  // We check for the user data dir in two different ways: directly, via the
  // profile manager, where it exists (it does not in unit tests), and via the
  // profile's directory, assuming the profile dir is a child of the user data
  // dir.
  std::vector<BlockPathRule> extra_rules;
  extra_rules.emplace_back(profile_->GetPath().DirName(), kBlockAllChildren);
  if (g_browser_process->profile_manager()) {
    extra_rules.emplace_back(
        g_browser_process->profile_manager()->user_data_dir(),
        kBlockAllChildren);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ShouldBlockAccessToPath, path_info.path, handle_type,
                     extra_rules),
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
    const content::PathInfo& path_info,
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
  if (handle_type == HandleType::kFile && user_action == UserAction::kSave) {
    // See https://crbug.com/1320877#c4 for justification for why we show the
    // prompt if `danger_level` is ALLOW_ON_USER_GESTURE as well as DANGEROUS.
    auto danger_level = GetFileTypeDangerLevel(
        path_info.path, origin, Profile::FromBrowserContext(profile_));
    if (danger_level == safe_browsing::DownloadFileType::DANGEROUS ||
        danger_level ==
            safe_browsing::DownloadFileType::ALLOW_ON_USER_GESTURE) {
      auto result_callback =
          base::BindPostTaskToCurrentDefault(std::move(callback));
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&ShowFileSystemAccessDangerousFileDialogOnUIThread,
                         frame_id, origin, path_info,
                         std::move(result_callback)));
      return;
    }
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
    const content::PathInfo& path_info) {
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
  entry.Set(kPathKey, base::FilePathToValue(path_info.path));
  entry.Set(kPathTypeKey, static_cast<int>(path_info.type));
  entry.Set(kDisplayNameKey, path_info.display_name);
  entry.Set(kTimestampKey, base::TimeToValue(clock_->Now()));

  dict.Set(GenerateLastPickedDirectoryKey(id), std::move(entry));

  MaybeEvictEntries(dict);

  content_settings_->SetWebsiteSettingDefaultScope(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
      base::Value(std::move(dict)));
}

content::PathInfo
ChromeFileSystemAccessPermissionContext::GetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value value = content_settings()->GetWebsiteSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_LAST_PICKED_DIRECTORY,
      /*info=*/nullptr);

  content::PathInfo path_info;
  if (!value.is_dict()) {
    return path_info;
  }

  auto* entry = value.GetDict().FindDict(GenerateLastPickedDirectoryKey(id));
  if (!entry) {
    return path_info;
  }

  auto type_int = entry->FindInt(kPathTypeKey)
                      .value_or(static_cast<int>(content::PathType::kLocal));
  path_info.type = type_int == static_cast<int>(content::PathType::kExternal)
                       ? content::PathType::kExternal
                       : content::PathType::kLocal;
  path_info.path =
      base::ValueToFilePath(entry->Find(kPathKey)).value_or(base::FilePath());
  path_info.display_name = StringOrEmpty(entry->FindString(kDisplayNameKey));
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
  switch (options->type_specific_options->which()) {
    case blink::mojom::TypeSpecificFilePickerOptionsUnion::Tag::
        kDirectoryPickerOptions:
      title = l10n_util::GetStringUTF16(
          options->type_specific_options->get_directory_picker_options()
                  ->request_writable
              ? IDS_FILE_SYSTEM_ACCESS_CHOOSER_OPEN_WRITABLE_DIRECTORY_TITLE
              : IDS_FILE_SYSTEM_ACCESS_CHOOSER_OPEN_READABLE_DIRECTORY_TITLE);
      break;
    case blink::mojom::TypeSpecificFilePickerOptionsUnion::Tag::
        kSaveFilePickerOptions:
      title = l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_ACCESS_CHOOSER_OPEN_SAVE_FILE_TITLE);
      break;
    case blink::mojom::TypeSpecificFilePickerOptionsUnion::Tag::
        kOpenFilePickerOptions:
      break;
  }
  return title;
}

void ChromeFileSystemAccessPermissionContext::NotifyEntryMoved(
    const url::Origin& origin,
    const content::PathInfo& old_path,
    const content::PathInfo& new_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (old_path == new_path) {
    return;
  }

  bool updated = false;
  auto it = active_permissions_map_.find(origin);
  if (it != active_permissions_map_.end()) {
    // TODO(crbug.com/40245144): Consolidate superfluous child grants.
    PermissionGrantImpl::UpdateGrantPath(it->second.write_grants, old_path,
                                         new_path);
    PermissionGrantImpl::UpdateGrantPath(it->second.read_grants, old_path,
                                         new_path);
    updated = true;
  }
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    // Active grants are a subset of persisted grants, so we also need to update
    // persisted grants, in case it's not covered by `UpdateGrantPath()` above.
    const std::unique_ptr<Object> object =
        GetGrantedObject(origin, PathAsPermissionKey(old_path.path));
    if (object) {
      base::Value::Dict new_object = object->value.Clone();
      new_object.Set(kPermissionPathKey, base::FilePathToValue(new_path.path));
      new_object.Set(kPermissionDisplayNameKey, new_path.display_name);
      UpdateObjectPermission(origin, object->value, std::move(new_object));
      updated = true;
    }
  }

  if (updated) {
    ScheduleUsageIconUpdate();
  }
}

void ChromeFileSystemAccessPermissionContext::
    OnFileCreatedFromShowSaveFilePicker(const GURL& file_picker_binding_context,
                                        const storage::FileSystemURL& url) {
  file_created_from_show_save_file_picker_callback_list_.Notify(
      file_picker_binding_context, url);
}

base::CallbackListSubscription ChromeFileSystemAccessPermissionContext::
    AddFileCreatedFromShowSaveFilePickerCallback(
        FileCreatedFromShowSaveFilePickerCallbackList::CallbackType callback) {
  return file_created_from_show_save_file_picker_callback_list_.Add(
      std::move(callback));
}

ChromeFileSystemAccessPermissionContext::Grants
ChromeFileSystemAccessPermissionContext::ConvertObjectsToGrants(
    std::vector<std::unique_ptr<Object>> objects) {
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

void ChromeFileSystemAccessPermissionContext::
    CreatePersistedGrantsFromActiveGrants(const url::Origin& origin) {
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    auto origin_it = active_permissions_map_.find(origin);
    if (origin_it != active_permissions_map_.end()) {
      OriginState& origin_state = origin_it->second;
      for (auto& read_grant : origin_state.read_grants) {
        if (HasGrantedActivePermissionStatus(read_grant.second)) {
          read_grant.second->SetStatus(
              PermissionStatus::GRANTED,
              PersistedPermissionOptions::kUpdatePersistedPermission);
        }
      }
      for (auto& write_grant : origin_state.write_grants) {
        if (HasGrantedActivePermissionStatus(write_grant.second)) {
          write_grant.second->SetStatus(
              PermissionStatus::GRANTED,
              PersistedPermissionOptions::kUpdatePersistedPermission);
        }
      }
    }
  }
}

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

void ChromeFileSystemAccessPermissionContext::RevokeGrants(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool grant_revoked = false;
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    grant_revoked =
        ObjectPermissionContextBase::RevokeObjectPermissions(origin);
    content_settings_->SetContentSettingDefaultScope(
        origin.GetURL(), origin.GetURL(),
        ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION,
        ContentSetting::CONTENT_SETTING_DEFAULT);
  }

  if (RevokeActiveGrants(origin)) {
    grant_revoked = true;
  }

  if (grant_revoked) {
    ScheduleUsageIconUpdate();
  }
}

bool ChromeFileSystemAccessPermissionContext::OriginHasReadAccess(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First, check if an origin has read access granted via active permissions.
  auto it = active_permissions_map_.find(origin);
  if (it != active_permissions_map_.end()) {
    return base::ranges::any_of(it->second.read_grants, [&](const auto& grant) {
      return HasGrantedActivePermissionStatus(grant.second);
    });
  }
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return false;
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

bool ChromeFileSystemAccessPermissionContext::OriginHasWriteAccess(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First, check if an origin has write access granted via active permissions.
  auto it = active_permissions_map_.find(origin);
  if (it != active_permissions_map_.end()) {
    return base::ranges::any_of(
        it->second.write_grants, [&](const auto& grant) {
          return HasGrantedActivePermissionStatus(grant.second);
        });
  }
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return false;
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

// All tabs for a given origin have been backgrounded or cleared in the past
// 16 hours. When this happens, we update the given origin's `OriginState` to
// note that all tabs were recently backgrounded.
void ChromeFileSystemAccessPermissionContext::OnAllTabsInBackgroundTimerExpired(
    const url::Origin& origin,
    const OneTimePermissionsTrackerObserver::BackgroundExpiryType&
        expiry_type) {
  if (
#if !BUILDFLAG(IS_ANDROID)
      !base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions) ||
#endif
      expiry_type != BackgroundExpiryType::kLongTimeout) {
    return;
  }
  SetPersistedGrantStatus(origin, PersistedGrantStatus::kBackgrounded);
  if (RevokeActiveGrants(origin)) {
    permissions::PermissionUmaUtil::RecordOneTimePermissionEvent(
        ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
        permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND);
    ScheduleUsageIconUpdate();
  }
}

void ChromeFileSystemAccessPermissionContext::OnLastPageFromOriginClosed(
    const url::Origin& origin) {
  CleanupPermissions(origin);
}

void ChromeFileSystemAccessPermissionContext::OnShutdown() {
  one_time_permissions_tracker_.Reset();
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeFileSystemAccessPermissionContext::OnWebAppInstalled(
    const webapps::AppId& app_id) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return;
  }

  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(profile()));
  const auto& registrar = provider->registrar_unsafe();
  if (!registrar.IsActivelyInstalled(app_id)) {
    return;
  }

  // TODO(crbug.com/40283362): Ensure that `GetAppScope` retrieves the correct
  // GURL when Scope Extensions is launched, which allows web apps to have more
  // than one origin as a scope.
  const auto gurl = registrar.GetAppScope(app_id);
  if (!gurl.is_valid()) {
    return;
  }
  const auto origin = url::Origin::Create(gurl);
  auto origin_it = active_permissions_map_.find(origin);
  if (origin_it == active_permissions_map_.end()) {
    // Ignore the origin if it does not have any active permissions.
    return;
  }

  // Update the cache value for web app state.
  OriginState& origin_state = origin_it->second;
  origin_state.web_app_install_status = WebAppInstallStatus::kInstalled;

  // Update the persisted grants, if needed.
  auto content_setting_value = content_settings_->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION);
  if (content_setting_value == ContentSetting::CONTENT_SETTING_ALLOW ||
      content_setting_value == ContentSetting::CONTENT_SETTING_BLOCK) {
    // The user has already enabled or disabled extended permissions from the
    // Restore Prompt or Page Info bubble. Installing a WebApp should not
    // change the extended permission state.
    return;
  }
  UpgradeToExtendedPermission(origin);
}

void ChromeFileSystemAccessPermissionContext::OnWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  // TODO(crbug.com/340952100): Remove the method after the InstallState is
  // saved in the database & available from OnWebAppInstalled.
  OnWebAppInstalled(app_id);
}

void ChromeFileSystemAccessPermissionContext::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return;
  }

  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(profile()));
  const auto& registrar = provider->registrar_unsafe();
  auto gurl = registrar.GetAppScope(app_id);
  if (!gurl.is_valid()) {
    return;
  }
  const auto origin = url::Origin::Create(gurl);
  auto origin_it = active_permissions_map_.find(origin);
  if (origin_it == active_permissions_map_.end()) {
    // Ignore the origin if it does not have any active permission.
    return;
  }

  // Update the cache value for web app state.
  OriginState& origin_state = origin_it->second;
  origin_state.web_app_install_status = WebAppInstallStatus::kUninstalled;

  // Update the persisted grants, if needed.
  auto content_setting_value = content_settings_->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION);
  if (content_setting_value == ContentSetting::CONTENT_SETTING_ALLOW ||
      content_setting_value == ContentSetting::CONTENT_SETTING_BLOCK) {
    // The user has already enabled or disabled extended permissions from the
    // Restore Prompt or Page Info bubble. Uninstalling a WebApp should not
    // change the extended permission state.
    return;
  }
  RemoveExtendedPermission(origin);
}

void ChromeFileSystemAccessPermissionContext::
    OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}
#endif

void ChromeFileSystemAccessPermissionContext::NavigatedAwayFromOrigin(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    auto it = active_permissions_map_.find(origin);
    // If we have no permissions for the origin, there is nothing to do.
    if (it == active_permissions_map_.end()) {
      return;
    }

    // Start a timer to possibly clean up permissions for this origin.
    if (!it->second.cleanup_timer) {
      it->second.cleanup_timer = std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE, kPermissionRevocationTimeout,
          base::BindRepeating(
              &ChromeFileSystemAccessPermissionContext::MaybeCleanupPermissions,
              base::Unretained(this), origin));
    }
    it->second.cleanup_timer->Reset();
  }
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

void ChromeFileSystemAccessPermissionContext::MaybeCleanupPermissions(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Iterate over all top-level frames by iterating over all tabs in all browser
  // windows. This also counts PWAs in windows without tab strips.
#if BUILDFLAG(IS_ANDROID)
  for (TabModel* tabs : TabModelList::models()) {
    if (tabs->GetProfile() != profile()) {
      continue;
    }
    int tab_count = tabs->GetTabCount();
#else
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile()) {
      continue;
    }
    TabStripModel* tabs = browser->tab_strip_model();
    int tab_count = tabs->count();
#endif
    for (int i = 0; i < tab_count; ++i) {
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
  CleanupPermissions(origin);
}

void ChromeFileSystemAccessPermissionContext::CleanupPermissions(
    const url::Origin& origin) {
  // TODO(crbug.com/40101962): Remove this custom implementation to handle site
  // navigation, with the launch of Persistent Permissions.
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    // Clear the grants that should not be carried across sessions.
    if (!OriginHasExtendedPermission(origin) &&
        GetPersistedGrantStatus(origin) == PersistedGrantStatus::kLoaded) {
      RevokeObjectPermissions(origin);
    }
    // Reset the persisted grant status to the default state.
    SetPersistedGrantStatus(origin, PersistedGrantStatus::kLoaded);
  }
  // Revoke the active grants, setting the status to `ASK`.
  if (RevokeActiveGrants(origin)) {
    ScheduleUsageIconUpdate();
  }
}

void ChromeFileSystemAccessPermissionContext::
    OnRestorePermissionAllowedEveryTime(const url::Origin& origin) {
  UpdateGrantsOnRestorePermissionAllowed(origin);
  content_settings_->SetContentSettingDefaultScope(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION,
      ContentSetting::CONTENT_SETTING_ALLOW);
}

void ChromeFileSystemAccessPermissionContext::OnRestorePermissionAllowedOnce(
    const url::Origin& origin) {
  UpdateGrantsOnRestorePermissionAllowed(origin);
}

void ChromeFileSystemAccessPermissionContext::
    UpdateGrantsOnRestorePermissionAllowed(const url::Origin& origin) {
  // Set `PersistedGrantStatus::kCurrent` so that Persisted grants are now
  // updated from dormant grants to extended/shadow grants.
  SetPersistedGrantStatus(origin, PersistedGrantStatus::kCurrent);

  auto it = active_permissions_map_.find(origin);
  if (it == active_permissions_map_.end()) {
    return;
  }
  // Use the persisted grants to find the matching active permission, and
  // set it to `granted`.
  for (auto& dormant_grant :
       ObjectPermissionContextBase::GetGrantedObjects(origin)) {
    base::Value::Dict& object_dict = dormant_grant->value;
    base::FilePath path =
        base::ValueToFilePath(object_dict.Find(kPermissionPathKey)).value();
    auto handle_type = object_dict.FindBool(kPermissionIsDirectoryKey).value()
                           ? HandleType::kDirectory
                           : HandleType::kFile;
    if (object_dict.FindBool(kPermissionReadableKey).value_or(false)) {
      auto& read_grant = it->second.read_grants[path];
      if (read_grant && read_grant->handle_type() == handle_type) {
        read_grant->SetStatus(
            PermissionStatus::GRANTED,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      }
    }
    if (object_dict.FindBool(kPermissionWritableKey).value_or(false)) {
      auto& write_grant = it->second.write_grants[path];
      if (write_grant && write_grant->handle_type() == handle_type) {
        write_grant->SetStatus(
            PermissionStatus::GRANTED,
            PersistedPermissionOptions::kDoNotUpdatePersistedPermission);
      }
    }
  }
}

void ChromeFileSystemAccessPermissionContext::
    OnRestorePermissionDeniedOrDismissed(const url::Origin& origin) {
  // Both denying and dismissing the restore prompt count as a `dismiss`
  // action, for embargo purposes.
  PermissionDecisionAutoBlockerFactory::GetForProfile(
      Profile::FromBrowserContext(profile()))
      ->RecordDismissAndEmbargo(
          origin.GetURL(),
          ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION, false);
  UpdateGrantsOnRestorePermissionNotAllowed(origin);
}

void ChromeFileSystemAccessPermissionContext::OnRestorePermissionIgnored(
    const url::Origin& origin) {
  PermissionDecisionAutoBlockerFactory::GetForProfile(
      Profile::FromBrowserContext(profile()))
      ->RecordIgnoreAndEmbargo(
          origin.GetURL(),
          ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION, false);
  UpdateGrantsOnRestorePermissionNotAllowed(origin);
}

void ChromeFileSystemAccessPermissionContext::
    UpdateGrantsOnRestorePermissionNotAllowed(const url::Origin& origin) {
  SetPersistedGrantStatus(origin, PersistedGrantStatus::kCurrent);
  // Revoke all of the persistent permissions for the given origin.
  if (!OriginHasExtendedPermission(origin)) {
    ObjectPermissionContextBase::RevokeObjectPermissions(origin);
  }
}

void ChromeFileSystemAccessPermissionContext::
    UpdateGrantsOnPermissionRequestResult(const url::Origin& origin) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return;
  }

  if (GetPersistedGrantStatus(origin) != PersistedGrantStatus::kCurrent) {
    // Requesting permission triggered the regular permission prompt, not the
    // restore permission prompt. Clear persisted grants and reset the grant
    // status so that dormant grants are not carried over to the next session.
    SetPersistedGrantStatus(origin, PersistedGrantStatus::kCurrent);
    if (!OriginHasExtendedPermission(origin)) {
      ObjectPermissionContextBase::RevokeObjectPermissions(origin);
    }
  }
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
        HasGrantedActivePermissionStatus(i->second)) {
      return true;
    }
  }
  return false;
}

bool ChromeFileSystemAccessPermissionContext::HasGrantedActivePermissionStatus(
    PermissionGrantImpl* grant) const {
  return grant &&
         grant->GetActivePermissionStatus() == PermissionStatus::GRANTED;
}

bool ChromeFileSystemAccessPermissionContext::
    IsEligibleToUpgradePermissionRequestToRestorePrompt(
        const url::Origin& origin,
        const base::FilePath& file_path,
        HandleType handle_type,
        UserAction user_action,
        GrantType grant_type) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40101963): Enable when android persisted permissions are
  // implemented.
  return false;
#else
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return false;
  }
  const bool origin_is_embargoed =
      PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(profile()))
          ->IsEmbargoed(
              origin.GetURL(),
              ContentSettingsType::FILE_SYSTEM_ACCESS_RESTORE_PERMISSION);
  if (origin_is_embargoed) {
    return false;
  }

  if (GetPersistedGrantType(origin) != PersistedGrantType::kDormant) {
    return false;
  }

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
  // The restore prompt is not displayed when there is a platform app installed,
  // because there is no valid UI element to display the restore prompt from.
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  const extensions::Extension* app =
      registry ? registry->enabled_extensions().GetExtensionOrAppByURL(
                     origin.GetURL())
               : nullptr;
  if (app && app->is_platform_app()) {
    return false;
  }
#endif

  // While this method is called from `RequestPermission`, which implies that
  // a `PermissionGrantImpl` exists - we want to insert the origin into the
  // permissions map if it does not exist, in order to cover cases of shutdown
  // or page navigation.
  auto& origin_state = active_permissions_map_[origin];

  // If an origin's grants have been revoked from being backgrounded, or
  // the permission request is on a handle retrieved from IndexedDB, then
  // the restore prompt may be eligible if requesting a permission on a handle,
  // which is previously granted (i.e. dormant grant exists for this file path).
  if (origin_state.persisted_grant_status ==
          PersistedGrantStatus::kBackgrounded ||
      user_action == UserAction::kLoadFromStorage) {
    return HasPersistedGrantObject(origin, file_path, handle_type, grant_type);
  }

  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

std::vector<FileRequestData> ChromeFileSystemAccessPermissionContext::
    GetFileRequestDataForRestorePermissionPrompt(const url::Origin& origin) {
  std::vector<FileRequestData> file_request_data_list;
  auto dormant_grants = ObjectPermissionContextBase::GetGrantedObjects(origin);
  for (auto& dormant_grant : dormant_grants) {
    if (!IsValidObject(dormant_grant->value)) {
      continue;
    }
    const base::Value::Dict& object_dict = dormant_grant->value;
    FileRequestData file_request_data = {
        content::PathInfo(
            base::ValueToFilePath(object_dict.Find(kPermissionPathKey)).value(),
            StringOrEmpty(object_dict.FindString(kPermissionDisplayNameKey))),
        object_dict.FindBool(kPermissionIsDirectoryKey).value_or(false)
            ? HandleType::kDirectory
            : HandleType::kFile,
        object_dict.FindBool(kPermissionWritableKey).value_or(false)
            ? RequestAccess::kWrite
            : RequestAccess::kRead};
    file_request_data_list.push_back(file_request_data);
  }
  return file_request_data_list;
}

bool ChromeFileSystemAccessPermissionContext::HasPersistedGrantObject(
    const url::Origin& origin,
    const base::FilePath& file_path,
    HandleType handle_type,
    GrantType grant_type) {
  auto persisted_grants =
      ObjectPermissionContextBase::GetGrantedObjects(origin);
  return base::ranges::any_of(persisted_grants, [&](const auto& object) {
    return HasMatchingValue(object->value, file_path, handle_type, grant_type);
  });
}

bool ChromeFileSystemAccessPermissionContext::HasMatchingValue(
    const base::Value::Dict& value,
    const base::FilePath& file_path,
    HandleType handle_type,
    GrantType grant_type) {
  return ValueToFilePath(value.Find(kPermissionPathKey)).value() == file_path &&
         value.FindBool(kPermissionIsDirectoryKey).value_or(false) ==
             (handle_type == HandleType::kDirectory) &&
         value.FindBool(GetGrantKeyFromGrantType(grant_type)).value_or(false);
}

void ChromeFileSystemAccessPermissionContext::
    SetOriginHasExtendedPermissionForTesting(const url::Origin& origin) {
  CHECK(base::FeatureList::IsEnabled(
      features::kFileSystemAccessPersistentPermissions));
  content_settings_->SetContentSettingDefaultScope(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION,
      ContentSetting::CONTENT_SETTING_ALLOW);
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::
    GetExtendedReadPermissionGrantForTesting(  // IN-TEST
        const url::Origin& origin,
        const content::PathInfo& path_info,
        HandleType handle_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto grant =
      GetReadPermissionGrant(origin, path_info, handle_type, UserAction::kOpen);

  static_cast<PermissionGrantImpl*>(grant.get())
      ->SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
  return grant;
}

scoped_refptr<content::FileSystemAccessPermissionGrant>
ChromeFileSystemAccessPermissionContext::
    GetExtendedWritePermissionGrantForTesting(  // IN-TEST
        const url::Origin& origin,
        const content::PathInfo& path_info,
        HandleType handle_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto grant = GetWritePermissionGrant(origin, path_info, handle_type,
                                       UserAction::kSave);

  static_cast<PermissionGrantImpl*>(grant.get())
      ->SetStatus(PermissionStatus::GRANTED,
                  PersistedPermissionOptions::kUpdatePersistedPermission);
  return grant;
}

void ChromeFileSystemAccessPermissionContext::Shutdown() {
  FlushScheduledSaveSettingsCalls();
  permissions::ObjectPermissionContextBase::Shutdown();
}

bool ChromeFileSystemAccessPermissionContext::
    CanAutoGrantViaPersistentPermission(const url::Origin& origin,
                                        const base::FilePath& path,
                                        HandleType handle_type,
                                        GrantType grant_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return false;
  }

  auto persisted_grant_type = GetPersistedGrantType(origin);
  if (!(persisted_grant_type == PersistedGrantType::kExtended ||
        persisted_grant_type == PersistedGrantType::kShadow)) {
    // Only shadow or extended grants are auto-granted.
    return false;
  }

  auto object = GetGrantedObject(origin, PathAsPermissionKey(path));
  return object &&
         HasMatchingValue(object->value, path, handle_type, grant_type);
}

bool ChromeFileSystemAccessPermissionContext::
    CanAutoGrantViaAncestorPersistentPermission(const url::Origin& origin,
                                                const base::FilePath& path,
                                                GrantType grant_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return false;
  }

  auto persisted_grant_type = GetPersistedGrantType(origin);
  if (!(persisted_grant_type == PersistedGrantType::kExtended ||
        persisted_grant_type == PersistedGrantType::kShadow)) {
    // Only shadow or extended grants are auto-granted.
    return false;
  }

  if (GetGrantedObjects(origin).empty()) {
    // Return early if the origin does not have any grant objects.
    return false;
  }

  for (base::FilePath parent = path.DirName(); parent != parent.DirName();
       parent = parent.DirName()) {
    auto object = GetGrantedObject(origin, PathAsPermissionKey(parent));
    if (object && HasMatchingValue(object->value, parent,
                                   HandleType::kDirectory, grant_type)) {
      return true;
    }
  }
  return false;
}

bool ChromeFileSystemAccessPermissionContext::OriginHasExtendedPermission(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40101963): Enable when android persisted permissions are
  // implemented.
  return false;
#else
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return false;
  }

  auto content_setting_value = content_settings_->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION);
  if (content_setting_value == ContentSetting::CONTENT_SETTING_ALLOW) {
    return true;
  }
  if (content_setting_value == ContentSetting::CONTENT_SETTING_BLOCK) {
    return false;
  }

  // If user has not set the extended permission preference, the extended
  // permission state depends on whether the origin has an web app actively
  // installed. First, check the cached value.
  auto& origin_state = active_permissions_map_[origin];
  if (origin_state.web_app_install_status != WebAppInstallStatus::kUnknown) {
    return origin_state.web_app_install_status ==
           WebAppInstallStatus::kInstalled;
  }
  // No cached value for web app install status. Retrieve the install status.
  DCHECK(profile());
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(profile()));
  if (!web_app_provider) {
    return false;
  }
  auto app_id = web_app_provider->registrar_unsafe().FindAppWithUrlInScope(
      origin.GetURL());
  auto has_actively_installed_app =
      app_id.has_value() &&
      web_app_provider->registrar_unsafe().IsActivelyInstalled(app_id.value());
  // Update the cached value.
  origin_state.web_app_install_status = has_actively_installed_app
                                            ? WebAppInstallStatus::kInstalled
                                            : WebAppInstallStatus::kUninstalled;
  return has_actively_installed_app;
#endif  // BUILDFLAG(IS_ANDROID)
}

void ChromeFileSystemAccessPermissionContext::SetOriginExtendedPermissionByUser(
    const url::Origin& origin) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return;
  }
  const bool has_extended_permission = OriginHasExtendedPermission(origin);
  content_settings_->SetContentSettingDefaultScope(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION,
      ContentSetting::CONTENT_SETTING_ALLOW);
  // Only update object permissions in the case that the origin did not
  // already have extended permissions.
  if (!has_extended_permission) {
    UpgradeToExtendedPermission(origin);
  }
}

void ChromeFileSystemAccessPermissionContext::
    RemoveOriginExtendedPermissionByUser(const url::Origin& origin) {
  if (!base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    return;
  }
  const bool has_extended_permission = OriginHasExtendedPermission(origin);
  content_settings_->SetContentSettingDefaultScope(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION,
      ContentSetting::CONTENT_SETTING_BLOCK);
  // Only update object permissions in the case that the origin already had
  // extended permissions.
  if (has_extended_permission) {
    RemoveExtendedPermission(origin);
  }
}

void ChromeFileSystemAccessPermissionContext::RemoveExtendedPermission(
    const url::Origin& origin) {
  auto origin_it = active_permissions_map_.find(origin);
  if (origin_it == active_permissions_map_.end()) {
    // Ignore the origin if it does not have any active permissions.
    return;
  }
  OriginState& origin_state = origin_it->second;
  // Re-create shadow grants based on active grants.
  RevokeObjectPermissions(origin);
  CreatePersistedGrantsFromActiveGrants(origin);
  ScheduleUsageIconUpdate();
  origin_state.persisted_grant_status = PersistedGrantStatus::kCurrent;
}

void ChromeFileSystemAccessPermissionContext::UpgradeToExtendedPermission(
    const url::Origin& origin) {
  auto origin_it = active_permissions_map_.find(origin);
  if (origin_it == active_permissions_map_.end()) {
    // Ignore the origin if it does not have any active permissions.
    return;
  }
  OriginState& origin_state = origin_it->second;
  if (origin_state.persisted_grant_status == PersistedGrantStatus::kCurrent) {
    // Previously, the given origin's persisted grants were shadow grants, and
    // installing a WebApp or enabling extended permissions from the Page Info
    // UI promotes these grants to extended grants.
    // The persisted grants are not affected, given that they are now
    // considered extended grants.
    return;
  }
  // Previously, the given origin's persisted grants were dormant grants and
  // therefore should not be promoted to extended grants. The dormant grants
  // are cleared so that they cannot be considered extended grants.
  RevokeObjectPermissions(origin);
  ScheduleUsageIconUpdate();
  origin_state.persisted_grant_status = PersistedGrantStatus::kCurrent;
}

ChromeFileSystemAccessPermissionContext::PersistedGrantType
ChromeFileSystemAccessPermissionContext::GetPersistedGrantType(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OriginHasExtendedPermission(origin)) {
    return PersistedGrantType::kExtended;
  }

  switch (GetPersistedGrantStatus(origin)) {
    case PersistedGrantStatus::kBackgrounded:
    case PersistedGrantStatus::kLoaded:
      return PersistedGrantType::kDormant;
    case PersistedGrantStatus::kCurrent:
      return PersistedGrantType::kShadow;
  }
}

PersistedGrantStatus
ChromeFileSystemAccessPermissionContext::GetPersistedGrantStatus(
    const url::Origin& origin) const {
  auto origin_it = active_permissions_map_.find(origin);
  if (origin_it != active_permissions_map_.end()) {
    return origin_it->second.persisted_grant_status;
  }
  // Return the default persisted grant status in the case that the origin is
  // not found in the active permissions map.
  return PersistedGrantStatus::kLoaded;
}

void ChromeFileSystemAccessPermissionContext::SetPersistedGrantStatus(
    const url::Origin& origin,
    PersistedGrantStatus persisted_grant_status) {
  // Insert the origin into the permissions map if it does not exist.
  auto& origin_state = active_permissions_map_[origin];
  origin_state.persisted_grant_status = persisted_grant_status;
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
    DCHECK_EQ(PermissionStatus::DENIED, grant->GetActivePermissionStatus());
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
