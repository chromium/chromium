// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_action.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

void BrowserAction::Cancel(crosapi::mojom::CreationResult reason) {
  DCHECK_NE(reason, mojom::CreationResult::kSuccess);
}

void BrowserAction::OnPerformed(BrowserManagerCallback on_performed,
                                mojom::CreationResult result) {
  const bool retry = result == mojom::CreationResult::kBrowserShutdown;
  std::move(on_performed).Run(retry);
}

// No-op action, used to start the browser without opening a window.
class NoOpAction final : public BrowserAction {
 public:
  NoOpAction() : BrowserAction(true) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {}
};

class NewWindowAction final : public BrowserAction {
 public:
  NewWindowAction(bool incognito,
                  bool should_trigger_session_restore,
                  int64_t target_display_id)
      : BrowserAction(true),
        incognito_(incognito),
        should_trigger_session_restore_(should_trigger_session_restore),
        target_display_id_(target_display_id),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    if (incognito_) {
      Profile* profile = ProfileManager::GetPrimaryUserProfile();
      if (!profile || !IncognitoModePrefs::IsIncognitoAllowed(profile))
        return;
    }
    service.service->NewWindow(incognito_, should_trigger_session_restore_,
                               target_display_id_,
                               base::BindOnce(&NewWindowAction::OnPerformed,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              std::move(on_performed)));
  }

 private:
  const bool incognito_;
  const bool should_trigger_session_restore_;
  const int64_t target_display_id_;
  base::WeakPtrFactory<NewWindowAction> weak_ptr_factory_;
};

class NewWindowForDetachingTabAction final : public BrowserAction {
 public:
  NewWindowForDetachingTabAction(base::StringPiece16 tab_id_str,
                                 base::StringPiece16 group_id_str,
                                 NewWindowForDetachingTabCallback callback)
      : BrowserAction(false),
        tab_id_str_(tab_id_str),
        group_id_str_(group_id_str),
        callback_(std::move(callback)),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    if (service.interface_version <
        mojom::BrowserService::kNewWindowForDetachingTabMinVersion) {
      Cancel(crosapi::mojom::CreationResult::kUnsupported);
      return;
    }

    service.service->NewWindowForDetachingTab(
        tab_id_str_, group_id_str_,
        base::BindOnce(&NewWindowForDetachingTabAction::OnPerformed,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(on_performed)));
  }

  void Cancel(crosapi::mojom::CreationResult reason) override {
    DCHECK_NE(reason, mojom::CreationResult::kSuccess);
    std::move(callback_).Run(reason, {});
  }

 private:
  const std::u16string tab_id_str_;
  const std::u16string group_id_str_;
  NewWindowForDetachingTabCallback callback_;
  base::WeakPtrFactory<NewWindowForDetachingTabAction> weak_ptr_factory_;

  void OnPerformed(BrowserManagerCallback on_performed,
                   mojom::CreationResult result,
                   const std::string& new_window) {
    const bool retry = result == mojom::CreationResult::kBrowserShutdown;
    if (!retry) {
      std::move(callback_).Run(result, new_window);
    }
    std::move(on_performed).Run(retry);
  }
};

class NewTabAction final : public BrowserAction {
 public:
  NewTabAction() : BrowserAction(true), weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    service.service->NewTab(base::BindOnce(&NewTabAction::OnPerformed,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(on_performed)));
  }

 private:
  base::WeakPtrFactory<NewTabAction> weak_ptr_factory_;
};

class LaunchAction final : public BrowserAction {
 public:
  explicit LaunchAction(int64_t target_display_id)
      : BrowserAction(true),
        target_display_id_(target_display_id),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    if (service.interface_version < mojom::BrowserService::kLaunchMinVersion) {
      LOG(WARNING)
          << "Lacros too old for Launch action - falling back to NewTab";
      service.service->NewTab(base::BindOnce(&LaunchAction::OnPerformed,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             std::move(on_performed)));
      return;
    }
    service.service->Launch(target_display_id_,
                            base::BindOnce(&LaunchAction::OnPerformed,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(on_performed)));
  }

 private:
  int64_t target_display_id_;
  base::WeakPtrFactory<LaunchAction> weak_ptr_factory_;
};

namespace {
crosapi::mojom::OpenUrlParams_SwitchToTabPathBehavior ConvertPathBehavior(
    NavigateParams::PathBehavior path_behavior) {
  switch (path_behavior) {
    case NavigateParams::RESPECT:
      return crosapi::mojom::OpenUrlParams_SwitchToTabPathBehavior::kRespect;
    case NavigateParams::IGNORE_AND_NAVIGATE:
      return crosapi::mojom::OpenUrlParams_SwitchToTabPathBehavior::kIgnore;
  }
}
}  // namespace

class OpenUrlAction final : public BrowserAction {
 public:
  OpenUrlAction(
      const GURL& url,
      crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
      crosapi::mojom::OpenUrlFrom from,
      NavigateParams::PathBehavior path_behavior)
      : BrowserAction(true),
        url_(url),
        disposition_(disposition),
        from_(from),
        path_behavior_(path_behavior),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    if (service.interface_version < mojom::BrowserService::kOpenUrlMinVersion) {
      LOG(ERROR) << "BrowserService does not support OpenUrl";
      return;
    }
    auto params = crosapi::mojom::OpenUrlParams::New();
    params->disposition = disposition_;
    params->from = from_;
    params->path_behavior = ConvertPathBehavior(path_behavior_);
    service.service->OpenUrl(url_, std::move(params),
                             base::BindOnce(&OpenUrlAction::OnPerformed,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            std::move(on_performed)));
  }

 private:
  const GURL url_;
  const crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition_;
  const crosapi::mojom::OpenUrlFrom from_;
  const NavigateParams::PathBehavior path_behavior_;
  base::WeakPtrFactory<OpenUrlAction> weak_ptr_factory_;
};

class NewGuestWindowAction final : public BrowserAction {
 public:
  explicit NewGuestWindowAction(int64_t target_display_id)
      : BrowserAction(true),
        target_display_id_(target_display_id),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    if (service.interface_version <
        crosapi::mojom::BrowserService::kNewGuestWindowMinVersion) {
      return;
    }
    service.service->NewGuestWindow(
        target_display_id_, base::BindOnce(&NewGuestWindowAction::OnPerformed,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(on_performed)));
  }

 private:
  const int64_t target_display_id_;
  base::WeakPtrFactory<NewGuestWindowAction> weak_ptr_factory_;
};

class HandleTabScrubbingAction final : public BrowserAction {
 public:
  HandleTabScrubbingAction(float x_offset, bool is_fling_scroll_event)
      : BrowserAction(false),
        x_offset_(x_offset),
        is_fling_scroll_event_(is_fling_scroll_event) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    if (service.interface_version <
        crosapi::mojom::BrowserService::kHandleTabScrubbingMinVersion) {
      return;
    }
    service.service->HandleTabScrubbing(x_offset_, is_fling_scroll_event_);
  }

 private:
  const float x_offset_;
  const bool is_fling_scroll_event_;
};

class NewFullscreenWindowAction final : public BrowserAction {
 public:
  NewFullscreenWindowAction(const GURL& url,
                            int64_t target_display_id,
                            NewFullscreenWindowCallback callback)
      : BrowserAction(true),
        url_(url),
        target_display_id_(target_display_id),
        callback_(std::move(callback)),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    if (service.interface_version <
        crosapi::mojom::BrowserService::kNewFullscreenWindowMinVersion) {
      Cancel(crosapi::mojom::CreationResult::kUnsupported);
      return;
    }
    service.service->NewFullscreenWindow(
        url_, target_display_id_,
        base::BindOnce(&NewFullscreenWindowAction::OnPerformed,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(on_performed)));
  }

  void Cancel(crosapi::mojom::CreationResult reason) override {
    DCHECK_NE(reason, mojom::CreationResult::kSuccess);
    std::move(callback_).Run(reason);
  }

 private:
  const GURL url_;
  const int64_t target_display_id_;
  NewFullscreenWindowCallback callback_;
  base::WeakPtrFactory<NewFullscreenWindowAction> weak_ptr_factory_;

  void OnPerformed(BrowserManagerCallback on_performed,
                   mojom::CreationResult result) {
    const bool retry = result == mojom::CreationResult::kBrowserShutdown;
    if (!retry) {
      std::move(callback_).Run(result);
    }
    std::move(on_performed).Run(retry);
  }
};

class RestoreTabAction final : public BrowserAction {
 public:
  RestoreTabAction() : BrowserAction(true), weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    service.service->RestoreTab(base::BindOnce(&RestoreTabAction::OnPerformed,
                                               weak_ptr_factory_.GetWeakPtr(),
                                               std::move(on_performed)));
  }

 private:
  base::WeakPtrFactory<RestoreTabAction> weak_ptr_factory_;
};

class OpenForFullRestoreAction final : public BrowserAction {
 public:
  explicit OpenForFullRestoreAction(bool skip_crash_restore)
      : BrowserAction(true), skip_crash_restore_(skip_crash_restore) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    service.service->OpenForFullRestore(skip_crash_restore_);
  }

 private:
  const bool skip_crash_restore_;
};

class CreateBrowserWithRestoredDataAction final : public BrowserAction {
 public:
  CreateBrowserWithRestoredDataAction(
      const std::vector<GURL>& urls,
      const gfx::Rect& bounds,
      const std::vector<tab_groups::TabGroupInfo>& tab_group_infos,
      ui::WindowShowState show_state,
      int32_t active_tab_index,
      int32_t first_non_pinned_tab_index,
      base::StringPiece app_name,
      int32_t restore_window_id)
      : BrowserAction(true),
        urls_(urls),
        bounds_(bounds),
        tab_group_infos_(tab_group_infos),
        show_state_(show_state),
        active_tab_index_(active_tab_index),
        first_non_pinned_tab_index_(first_non_pinned_tab_index),
        app_name_(app_name),
        restore_window_id_(restore_window_id) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    crosapi::mojom::DeskTemplateStatePtr additional_state =
        crosapi::mojom::DeskTemplateState::New(
            urls_, active_tab_index_, app_name_, restore_window_id_,
            first_non_pinned_tab_index_, tab_group_infos_);
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->desk_template_ash()
        ->CreateBrowserWithRestoredData(bounds_, show_state_,
                                        std::move(additional_state));
  }

 private:
  const std::vector<GURL> urls_;
  const gfx::Rect bounds_;
  const std::vector<tab_groups::TabGroupInfo> tab_group_infos_;
  const ui::WindowShowState show_state_;
  const int32_t active_tab_index_;
  const int32_t first_non_pinned_tab_index_;
  const std::string app_name_;
  const int32_t restore_window_id_;
};

// static
std::unique_ptr<BrowserAction> BrowserAction::NewWindow(
    bool incognito,
    bool should_trigger_session_restore,
    int64_t target_display_id) {
  return std::make_unique<NewWindowAction>(
      incognito, should_trigger_session_restore, target_display_id);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::NewTab() {
  return std::make_unique<NewTabAction>();
}

// static
std::unique_ptr<BrowserAction> BrowserAction::Launch(
    int64_t target_display_id) {
  return std::make_unique<LaunchAction>(target_display_id);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::NewWindowForDetachingTab(
    base::StringPiece16 tab_id_str,
    base::StringPiece16 group_id_str,
    NewWindowForDetachingTabCallback callback) {
  return std::make_unique<NewWindowForDetachingTabAction>(
      tab_id_str, group_id_str, std::move(callback));
}

// static
std::unique_ptr<BrowserAction> BrowserAction::NewGuestWindow(
    int64_t target_display_id) {
  return std::make_unique<NewGuestWindowAction>(target_display_id);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::NewFullscreenWindow(
    const GURL& url,
    int64_t target_display_id,
    NewFullscreenWindowCallback callback) {
  return std::make_unique<NewFullscreenWindowAction>(url, target_display_id,
                                                     std::move(callback));
}

// static
std::unique_ptr<BrowserAction> BrowserAction::OpenUrl(
    const GURL& url,
    crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition,
    crosapi::mojom::OpenUrlFrom from,
    NavigateParams::PathBehavior path_behavior) {
  return std::make_unique<OpenUrlAction>(url, disposition, from, path_behavior);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::OpenForFullRestore(
    bool skip_crash_restore) {
  return std::make_unique<OpenForFullRestoreAction>(skip_crash_restore);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::RestoreTab() {
  return std::make_unique<RestoreTabAction>();
}

// static
std::unique_ptr<BrowserAction> BrowserAction::HandleTabScrubbing(
    float x_offset,
    bool is_fling_scroll_event) {
  return std::make_unique<HandleTabScrubbingAction>(x_offset,
                                                    is_fling_scroll_event);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::CreateBrowserWithRestoredData(
    const std::vector<GURL>& urls,
    const gfx::Rect& bounds,
    const std::vector<tab_groups::TabGroupInfo>& tab_groups,
    ui::WindowShowState show_state,
    int32_t active_tab_index,
    int32_t first_non_pinned_tab_index,
    base::StringPiece app_name,
    int32_t restore_window_id) {
  return std::make_unique<CreateBrowserWithRestoredDataAction>(
      urls, bounds, tab_groups, show_state, active_tab_index,
      first_non_pinned_tab_index, app_name, restore_window_id);
}

// No window will be opened in the following circumstances:
// 1. Lacros-chrome is initialized in the Kiosk session
// 2. Full restore is responsible for restoring/launching Lacros.
// static
std::unique_ptr<BrowserAction> BrowserAction::GetActionForSessionStart() {
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
    return std::make_unique<NewWindowAction>(
        /*incognito=*/false, /*should_trigger_session_restore=*/false, -1);
  }
  if (chromeos::IsKioskSession() ||
      ash::full_restore::MaybeCreateFullRestoreServiceForLacros()) {
    return std::make_unique<NoOpAction>();
  }
  return std::make_unique<NewWindowAction>(
      /*incognito=*/false, /*should_trigger_session_restore=*/true, -1);
}

BrowserActionQueue::BrowserActionQueue() = default;
BrowserActionQueue::~BrowserActionQueue() = default;

bool BrowserActionQueue::IsEmpty() const {
  return actions_.empty();
}

void BrowserActionQueue::PushOrCancel(std::unique_ptr<BrowserAction> action,
                                      mojom::CreationResult cancel_reason) {
  if (action->IsQueueable()) {
    actions_.push(std::move(action));
  } else {
    action->Cancel(cancel_reason);
  }
}

void BrowserActionQueue::Push(std::unique_ptr<BrowserAction> action) {
  DCHECK(action->IsQueueable());
  actions_.push(std::move(action));
}

std::unique_ptr<BrowserAction> BrowserActionQueue::Pop() {
  DCHECK(!IsEmpty());
  std::unique_ptr<BrowserAction> action = std::move(actions_.front());
  actions_.pop();
  return action;
}

void BrowserActionQueue::Clear() {
  base::queue<std::unique_ptr<BrowserAction>> empty;
  actions_.swap(empty);
  DCHECK(IsEmpty());
}

}  // namespace crosapi
