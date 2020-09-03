// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/origin_scoped_native_file_system_permission_context.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/native_file_system_dialogs.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#endif

namespace {
using blink::mojom::PermissionStatus;
using permissions::PermissionAction;

enum class GrantType { kRead, kWrite };

// This long after the last top-level tab or window for an origin is closed (or
// is navigated to another origin), all the permissions for that origin will be
// revoked.
constexpr base::TimeDelta kPermissionRevocationTimeout =
    base::TimeDelta::FromSeconds(5);

}  // namespace

class OriginScopedNativeFileSystemPermissionContext::PermissionGrantImpl
    : public content::NativeFileSystemPermissionGrant {
  using HandleType = NativeFileSystemPermissionContext::HandleType;

 public:
  PermissionGrantImpl(
      base::WeakPtr<OriginScopedNativeFileSystemPermissionContext> context,
      const url::Origin& origin,
      const base::FilePath& path,
      HandleType handle_type,
      GrantType type)
      : context_(std::move(context)),
        origin_(origin),
        path_(path),
        handle_type_(handle_type),
        type_(type) {}

  // NativeFileSystemPermissionGrant:
  PermissionStatus GetStatus() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return status_;
  }
  base::FilePath GetPath() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_;
  }

  void RequestPermission(
      content::GlobalFrameRoutingId frame_id,
      UserActivationState user_activation_state,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Check if a permission request has already been processed previously. This
    // check is done first because we don't want to reset the status of a
    // permission if it has already been granted.
    if (GetStatus() != PermissionStatus::ASK || !context_) {
      std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
      return;
    }

    if (type_ == GrantType::kWrite) {
      ContentSetting content_setting =
          context_->GetWriteGuardContentSetting(origin_);

      // Content setting grants write permission without asking.
      if (content_setting == CONTENT_SETTING_ALLOW) {
        SetStatus(PermissionStatus::GRANTED);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback),
            PermissionRequestOutcome::kGrantedByContentSetting);
        return;
      }

      // Content setting blocks write permission.
      if (content_setting == CONTENT_SETTING_BLOCK) {
        SetStatus(PermissionStatus::DENIED);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback),
            PermissionRequestOutcome::kBlockedByContentSetting);
        return;
      }
    }

    // Otherwise, perform checks and ask the user for permission.

    content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
    if (!rfh || !rfh->IsCurrent()) {
      // Requested from a no longer valid render frame host.
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

    url::Origin embedding_origin =
        url::Origin::Create(web_contents->GetLastCommittedURL());
    if (embedding_origin != origin_) {
      // Third party iframes are not allowed to request more permissions.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kThirdPartyContext);
      return;
    }

    auto* request_manager =
        NativeFileSystemPermissionRequestManager::FromWebContents(web_contents);
    if (!request_manager) {
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kRequestAborted);
      return;
    }

    // Drop fullscreen mode so that the user sees the URL bar.
    base::ScopedClosureRunner fullscreen_block =
        web_contents->ForSecurityDropFullscreen();

    NativeFileSystemPermissionRequestManager::Access access =
        type_ == GrantType::kRead
            ? NativeFileSystemPermissionRequestManager::Access::kRead
            : NativeFileSystemPermissionRequestManager::Access::kWrite;

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

  const url::Origin& origin() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return origin_;
  }

  HandleType handle_type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return handle_type_;
  }

  const base::FilePath& path() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_;
  }

  GrantType type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return type_;
  }

  void SetStatus(PermissionStatus status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (status_ == status)
      return;
    status_ = status;
    NotifyPermissionStatusChanged();
  }

  static void CollectGrants(
      const std::map<base::FilePath, PermissionGrantImpl*>& grants,
      std::vector<base::FilePath>* directory_grants,
      std::vector<base::FilePath>* file_grants) {
    for (const auto& entry : grants) {
      if (entry.second->GetStatus() != PermissionStatus::GRANTED)
        continue;
      if (entry.second->handle_type() == HandleType::kDirectory) {
        directory_grants->push_back(entry.second->path());
      } else {
        file_grants->push_back(entry.second->path());
      }
    }
  }

 protected:
  ~PermissionGrantImpl() override {
    if (context_)
      context_->PermissionGrantDestroyed(this);
  }

 private:
  void OnPermissionRequestResult(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionAction result) {
    switch (result) {
      case PermissionAction::GRANTED:
        SetStatus(PermissionStatus::GRANTED);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserGranted);
        if (context_)
          context_->ScheduleUsageIconUpdate();
        break;
      case PermissionAction::DENIED:
        SetStatus(PermissionStatus::DENIED);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDenied);
        break;
      case PermissionAction::DISMISSED:
      case PermissionAction::IGNORED:
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDismissed);
        break;
      case PermissionAction::REVOKED:
      case PermissionAction::NUM:
        NOTREACHED();
        break;
    }
  }

  void RunCallbackAndRecordPermissionRequestOutcome(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionRequestOutcome outcome) {
    if (type_ == GrantType::kWrite) {
      base::UmaHistogramEnumeration(
          "NativeFileSystemAPI.WritePermissionRequestOutcome", outcome);
      if (handle_type_ == HandleType::kDirectory) {
        base::UmaHistogramEnumeration(
            "NativeFileSystemAPI.WritePermissionRequestOutcome.Directory",
            outcome);
      } else {
        base::UmaHistogramEnumeration(
            "NativeFileSystemAPI.WritePermissionRequestOutcome.File", outcome);
      }
    } else {
      base::UmaHistogramEnumeration(
          "NativeFileSystemAPI.ReadPermissionRequestOutcome", outcome);
      if (handle_type_ == HandleType::kDirectory) {
        base::UmaHistogramEnumeration(
            "NativeFileSystemAPI.ReadPermissionRequestOutcome.Directory",
            outcome);
      } else {
        base::UmaHistogramEnumeration(
            "NativeFileSystemAPI.ReadPermissionRequestOutcome.File", outcome);
      }
    }

    std::move(callback).Run(outcome);
  }

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<OriginScopedNativeFileSystemPermissionContext> const context_;
  const url::Origin origin_;
  const base::FilePath path_;
  const HandleType handle_type_;
  const GrantType type_;

  // This member should only be updated via SetStatus(), to make sure
  // observers are properly notified about any change in status.
  PermissionStatus status_ = PermissionStatus::ASK;
};

struct OriginScopedNativeFileSystemPermissionContext::OriginState {
  // Raw pointers, owned collectively by all the handles that reference this
  // grant. When last reference goes away this state is cleared as well by
  // PermissionGrantDestroyed().
  // TODO(mek): Revoke all permissions after the last tab for an origin gets
  // closed.
  std::map<base::FilePath, PermissionGrantImpl*> read_grants;
  std::map<base::FilePath, PermissionGrantImpl*> write_grants;

  // Timer that is triggered whenever the user navigates away from this origin.
  // This is used to give a website a little bit of time for background work
  // before revoking all permissions for the origin.
  std::unique_ptr<base::RetainingOneShotTimer> cleanup_timer;
};

OriginScopedNativeFileSystemPermissionContext::
    OriginScopedNativeFileSystemPermissionContext(
        content::BrowserContext* context)
    : ChromeNativeFileSystemPermissionContext(context), profile_(context) {}

OriginScopedNativeFileSystemPermissionContext::
    ~OriginScopedNativeFileSystemPermissionContext() = default;

scoped_refptr<content::NativeFileSystemPermissionGrant>
OriginScopedNativeFileSystemPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    content::NativeFileSystemPermissionContext::HandleType handle_type,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |origins_|, but that
  // is exactly what we want.
  auto& origin_state = origins_[origin];
  // TODO(https://crbug.com/984772): If a parent directory is already
  // readable this newly returned grant should also be readable.
  auto*& existing_grant = origin_state.read_grants[path];
  scoped_refptr<PermissionGrantImpl> new_grant;

  if (existing_grant && existing_grant->handle_type() != handle_type) {
    // |path| changed from being a directory to being a file or vice versa,
    // don't just re-use the existing grant but revoke the old grant before
    // creating a new grant.
    existing_grant->SetStatus(PermissionStatus::DENIED);
    existing_grant = nullptr;
  }

  if (!existing_grant) {
    new_grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path, handle_type,
        GrantType::kRead);
    existing_grant = new_grant.get();
  }

  const ContentSetting content_setting = GetReadGuardContentSetting(origin);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      existing_grant->SetStatus(PermissionStatus::GRANTED);
      break;
    case CONTENT_SETTING_ASK:
      switch (user_action) {
        case UserAction::kOpen:
        case UserAction::kSave:
          // Open and Save dialog only grant read access for individual files.
          if (handle_type == HandleType::kDirectory)
            break;
          FALLTHROUGH;
        case UserAction::kDragAndDrop:
          // Drag&drop grants read access for all handles.
          existing_grant->SetStatus(PermissionStatus::GRANTED);
          ScheduleUsageIconUpdate();
          break;
        case UserAction::kLoadFromStorage:
          break;
      }
      break;
    case CONTENT_SETTING_BLOCK:
      if (new_grant) {
        existing_grant->SetStatus(PermissionStatus::DENIED);
      } else {
        // We won't revoke permission to an existing grant.
        // TODO(crbug.com/1053363): Better integrate with content settings.
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  return existing_grant;
}

scoped_refptr<content::NativeFileSystemPermissionGrant>
OriginScopedNativeFileSystemPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |origins_|, but that
  // is exactly what we want.
  auto& origin_state = origins_[origin];
  // TODO(https://crbug.com/984772): If a parent directory is already
  // writable this newly returned grant should also be writable.
  auto*& existing_grant = origin_state.write_grants[path];
  scoped_refptr<PermissionGrantImpl> new_grant;

  if (existing_grant && existing_grant->handle_type() != handle_type) {
    // |path| changed from being a directory to being a file or vice versa,
    // don't just re-use the existing grant but revoke the old grant before
    // creating a new grant.
    existing_grant->SetStatus(PermissionStatus::DENIED);
    existing_grant = nullptr;
  }

  if (!existing_grant) {
    new_grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path, handle_type,
        GrantType::kWrite);
    existing_grant = new_grant.get();
  }

  const ContentSetting content_setting = GetWriteGuardContentSetting(origin);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      existing_grant->SetStatus(PermissionStatus::GRANTED);
      break;
    case CONTENT_SETTING_ASK:
      switch (user_action) {
        case UserAction::kSave:
          // Only automatically grant write access for save dialogs.
          existing_grant->SetStatus(PermissionStatus::GRANTED);
          ScheduleUsageIconUpdate();
          break;
        case UserAction::kOpen:
        case UserAction::kDragAndDrop:
        case UserAction::kLoadFromStorage:
          break;
      }
      break;
    case CONTENT_SETTING_BLOCK:
      if (new_grant) {
        existing_grant->SetStatus(PermissionStatus::DENIED);
      } else {
        // We won't revoke permission to an existing grant.
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  return existing_grant;
}

ChromeNativeFileSystemPermissionContext::Grants
OriginScopedNativeFileSystemPermissionContext::GetPermissionGrants(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return {};

  Grants grants;
  PermissionGrantImpl::CollectGrants(it->second.read_grants,
                                     &grants.directory_read_grants,
                                     &grants.file_read_grants);
  PermissionGrantImpl::CollectGrants(it->second.write_grants,
                                     &grants.directory_write_grants,
                                     &grants.file_write_grants);
  return grants;
}

void OriginScopedNativeFileSystemPermissionContext::RevokeGrants(
    const url::Origin& origin) {
  auto origin_it = origins_.find(origin);
  if (origin_it == origins_.end())
    return;

  OriginState& origin_state = origin_it->second;
  for (auto& grant : origin_state.read_grants)
    grant.second->SetStatus(PermissionStatus::ASK);
  for (auto& grant : origin_state.write_grants)
    grant.second->SetStatus(PermissionStatus::ASK);
  ScheduleUsageIconUpdate();
}

bool OriginScopedNativeFileSystemPermissionContext::OriginHasReadAccess(
    const url::Origin& origin) {
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return false;
  if (it->second.read_grants.empty())
    return false;
  for (const auto& grant : it->second.read_grants) {
    if (grant.second->GetStatus() == PermissionStatus::GRANTED)
      return true;
  }
  return false;
}

bool OriginScopedNativeFileSystemPermissionContext::OriginHasWriteAccess(
    const url::Origin& origin) {
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return false;
  if (it->second.write_grants.empty())
    return false;
  for (const auto& grant : it->second.write_grants) {
    if (grant.second->GetStatus() == PermissionStatus::GRANTED)
      return true;
  }
  return false;
}

void OriginScopedNativeFileSystemPermissionContext::NavigatedAwayFromOrigin(
    const url::Origin& origin) {
  auto it = origins_.find(origin);
  // If we have no permissions for the origin, there is nothing to do.
  if (it == origins_.end())
    return;

  // Start a timer to possibly clean up permissions for this origin.
  if (!it->second.cleanup_timer) {
    it->second.cleanup_timer = std::make_unique<base::RetainingOneShotTimer>(
        FROM_HERE, kPermissionRevocationTimeout,
        base::BindRepeating(&OriginScopedNativeFileSystemPermissionContext::
                                MaybeCleanupPermissions,
                            base::Unretained(this), origin));
  }
  it->second.cleanup_timer->Reset();
}

void OriginScopedNativeFileSystemPermissionContext::TriggerTimersForTesting() {
  for (const auto& it : origins_) {
    if (it.second.cleanup_timer) {
      auto task = it.second.cleanup_timer->user_task();
      it.second.cleanup_timer->Stop();
      task.Run();
    }
  }
}

void OriginScopedNativeFileSystemPermissionContext::MaybeCleanupPermissions(
    const url::Origin& origin) {
  auto it = origins_.find(origin);
  // If we have no permissions for the origin, there is nothing to do.
  if (it == origins_.end())
    return;

#if !defined(OS_ANDROID)
  // Iterate over all top-level frames by iterating over all browsers, and all
  // tabs within those browsers. This also counts PWAs in windows without
  // tab strips, as those are still implemented as a Browser with a single tab.
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile())
      continue;
    TabStripModel* tabs = browser->tab_strip_model();
    for (int i = 0; i < tabs->count(); ++i) {
      content::WebContents* web_contents = tabs->GetWebContentsAt(i);
      url::Origin tab_origin =
          url::Origin::Create(web_contents->GetLastCommittedURL());
      // Found a tab for this origin, so early exit and don't revoke grants.
      if (tab_origin == origin)
        return;
    }
  }

  // No tabs found with the same origin, so revoke all permissions for the
  // origin.
  RevokeGrants(origin);
#endif
}

void OriginScopedNativeFileSystemPermissionContext::PermissionGrantDestroyed(
    PermissionGrantImpl* grant) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(grant->origin());
  if (it == origins_.end())
    return;

  auto& grants = grant->type() == GrantType::kRead ? it->second.read_grants
                                                   : it->second.write_grants;
  auto grant_it = grants.find(grant->path());
  // Any non-denied permission grants should have still been in our grants list.
  // If this invariant is voilated we would have permissions that might be
  // granted but won't be visible in any UI because the permission context isn't
  // tracking them anymore.
  if (grant_it == grants.end()) {
    DCHECK_EQ(PermissionStatus::DENIED, grant->GetStatus());
    return;
  }

  // The grant in |grants| for this path might have been replaced with a
  // different grant. Only erase if it actually matches the grant that was
  // destroyed.
  if (grant_it->second == grant)
    grants.erase(grant_it);

  ScheduleUsageIconUpdate();
}

void OriginScopedNativeFileSystemPermissionContext::ScheduleUsageIconUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (usage_icon_update_scheduled_)
    return;
  usage_icon_update_scheduled_ = true;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OriginScopedNativeFileSystemPermissionContext::DoUsageIconUpdate,
          weak_factory_.GetWeakPtr()));
}

void OriginScopedNativeFileSystemPermissionContext::DoUsageIconUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_icon_update_scheduled_ = false;
#if !defined(OS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile())
      continue;
    browser->window()->UpdatePageActionIcon(
        PageActionIconType::kNativeFileSystemAccess);
  }
#endif
}

base::WeakPtr<ChromeNativeFileSystemPermissionContext>
OriginScopedNativeFileSystemPermissionContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
