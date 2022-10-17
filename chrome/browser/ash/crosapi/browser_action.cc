// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_action.h"

#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

// No-op action, used to start the browser without opening a window.
class NoOpAction final : public BrowserAction {
 public:
  NoOpAction() : BrowserAction(true) {}
  void Perform(const VersionedBrowserService& service) override {}
};

class NewWindowAction final : public BrowserAction {
 public:
  NewWindowAction(bool incognito,
                  bool should_trigger_session_restore,
                  int64_t target_display_id)
      : BrowserAction(true),
        incognito_(incognito),
        should_trigger_session_restore_(should_trigger_session_restore),
        target_display_id_(target_display_id) {}

  void Perform(const VersionedBrowserService& service) override {
    if (incognito_) {
      Profile* profile = ProfileManager::GetPrimaryUserProfile();
      if (!profile || !IncognitoModePrefs::IsIncognitoAllowed(profile))
        return;
    }
    service.service->NewWindow(incognito_, should_trigger_session_restore_,
                               target_display_id_, base::DoNothing());
  }

 private:
  const bool incognito_;
  const bool should_trigger_session_restore_;
  const int64_t target_display_id_;
};

class NewWindowForDetachingTabAction final : public BrowserAction {
 public:
  NewWindowForDetachingTabAction(base::StringPiece16 tab_id_str,
                                 base::StringPiece16 group_id_str,
                                 NewWindowForDetachingTabCallback callback)
      : BrowserAction(false),
        tab_id_str_(tab_id_str),
        group_id_str_(group_id_str),
        callback_(std::move(callback)) {}

  void Perform(const VersionedBrowserService& service) override {
    if (service.interface_version <
        mojom::BrowserService::kNewWindowForDetachingTabMinVersion) {
      Cancel(crosapi::mojom::CreationResult::kUnsupported);
      return;
    }
    service.service->NewWindowForDetachingTab(tab_id_str_, group_id_str_,
                                              std::move(callback_));
  }

  void Cancel(crosapi::mojom::CreationResult reason) override {
    std::move(callback_).Run(reason, std::string() /*new_window*/);
  }

 private:
  const std::u16string tab_id_str_;
  const std::u16string group_id_str_;
  NewWindowForDetachingTabCallback callback_;
};

class NewTabAction final : public BrowserAction {
 public:
  NewTabAction() : BrowserAction(true) {}

  void Perform(const VersionedBrowserService& service) override {
    service.service->NewTabWithoutParameter(base::DoNothing());
  }
};

class LaunchAction final : public BrowserAction {
 public:
  explicit LaunchAction(int64_t target_display_id)
      : BrowserAction(true), target_display_id_(target_display_id) {}

  void Perform(const VersionedBrowserService& service) override {
    if (service.interface_version < mojom::BrowserService::kLaunchMinVersion) {
      LOG(WARNING)
          << "Lacros too old for Launch action - falling back to NewTab";
      service.service->NewTabWithoutParameter(base::DoNothing());
      return;
    }
    service.service->Launch(target_display_id_, base::DoNothing());
  }

 private:
  int64_t target_display_id_;
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
        path_behavior_(path_behavior) {}

  void Perform(const VersionedBrowserService& service) override {
    if (service.interface_version < mojom::BrowserService::kOpenUrlMinVersion) {
      LOG(ERROR) << "BrowserService does not support OpenUrl";
      return;
    }
    auto params = crosapi::mojom::OpenUrlParams::New();
    params->disposition = disposition_;
    params->from = from_;
    params->path_behavior = ConvertPathBehavior(path_behavior_);
    service.service->OpenUrl(url_, std::move(params), base::DoNothing());
  }

 private:
  const GURL url_;
  const crosapi::mojom::OpenUrlParams::WindowOpenDisposition disposition_;
  const crosapi::mojom::OpenUrlFrom from_;
  const NavigateParams::PathBehavior path_behavior_;
};

class NewGuestWindowAction final : public BrowserAction {
 public:
  explicit NewGuestWindowAction(int64_t target_display_id)
      : BrowserAction(true), target_display_id_(target_display_id) {}

  void Perform(const VersionedBrowserService& service) override {
    if (service.interface_version <
        crosapi::mojom::BrowserService::kNewGuestWindowMinVersion) {
      return;
    }
    service.service->NewGuestWindow(target_display_id_, base::DoNothing());
  }

 private:
  const int64_t target_display_id_;
};

class HandleTabScrubbingAction final : public BrowserAction {
 public:
  explicit HandleTabScrubbingAction(float x_offset)
      : BrowserAction(false), x_offset_(x_offset) {}

  void Perform(const VersionedBrowserService& service) override {
    if (service.interface_version <
        crosapi::mojom::BrowserService::kHandleTabScrubbingMinVersion) {
      return;
    }
    service.service->HandleTabScrubbing(x_offset_);
  }

 private:
  const float x_offset_;
};

class NewFullscreenWindowAction final : public BrowserAction {
 public:
  NewFullscreenWindowAction(const GURL& url,
                            int64_t target_display_id,
                            NewFullscreenWindowCallback callback)
      : BrowserAction(true),
        url_(url),
        target_display_id_(target_display_id),
        callback_(std::move(callback)) {}

  void Perform(const VersionedBrowserService& service) override {
    if (service.interface_version <
        crosapi::mojom::BrowserService::kNewFullscreenWindowMinVersion) {
      Cancel(crosapi::mojom::CreationResult::kUnsupported);
      return;
    }
    service.service->NewFullscreenWindow(url_, target_display_id_,
                                         std::move(callback_));
  }

  void Cancel(crosapi::mojom::CreationResult reason) override {
    std::move(callback_).Run(reason);
  }

 private:
  const GURL url_;
  const int64_t target_display_id_;
  NewFullscreenWindowCallback callback_;
};

class RestoreTabAction final : public BrowserAction {
 public:
  RestoreTabAction() : BrowserAction(true) {}

  void Perform(const VersionedBrowserService& service) override {
    service.service->RestoreTab(base::DoNothing());
  }
};

class OpenForFullRestoreAction final : public BrowserAction {
 public:
  explicit OpenForFullRestoreAction(bool skip_crash_restore)
      : BrowserAction(true), skip_crash_restore_(skip_crash_restore) {}

  void Perform(const VersionedBrowserService& service) override {
    service.service->OpenForFullRestore(skip_crash_restore_);
  }

 private:
  const bool skip_crash_restore_;
};

namespace {
ui::mojom::WindowShowState ConvertWindowShowState(ui::WindowShowState state) {
  switch (state) {
    case ui::SHOW_STATE_DEFAULT:
      return ui::mojom::WindowShowState::SHOW_STATE_DEFAULT;
    case ui::SHOW_STATE_NORMAL:
      return ui::mojom::WindowShowState::SHOW_STATE_NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return ui::mojom::WindowShowState::SHOW_STATE_MINIMIZED;
    case ui::SHOW_STATE_MAXIMIZED:
      return ui::mojom::WindowShowState::SHOW_STATE_MAXIMIZED;
    case ui::SHOW_STATE_INACTIVE:
      return ui::mojom::WindowShowState::SHOW_STATE_INACTIVE;
    case ui::SHOW_STATE_FULLSCREEN:
      return ui::mojom::WindowShowState::SHOW_STATE_FULLSCREEN;
    case ui::SHOW_STATE_END:
      NOTREACHED();
      return ui::mojom::WindowShowState::SHOW_STATE_DEFAULT;
  }
}
}  // namespace

class CreateBrowserWithRestoredDataAction final : public BrowserAction {
 public:
  CreateBrowserWithRestoredDataAction(const std::vector<GURL>& urls,
                                      const gfx::Rect& bounds,
                                      ui::WindowShowState show_state,
                                      int32_t active_tab_index,
                                      base::StringPiece app_name,
                                      int32_t restore_window_id)
      : BrowserAction(true),
        urls_(urls),
        bounds_(bounds),
        show_state_(show_state),
        active_tab_index_(active_tab_index),
        app_name_(app_name),
        restore_window_id_(restore_window_id) {}

  void Perform(const VersionedBrowserService& service) override {
    crosapi::mojom::DeskTemplateStatePtr additional_state =
        crosapi::mojom::DeskTemplateState::New(urls_, active_tab_index_,
                                               app_name_, restore_window_id_);
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->desk_template_ash()
        ->CreateBrowserWithRestoredData(bounds_,
                                        ConvertWindowShowState(show_state_),
                                        std::move(additional_state));
  }

 private:
  const std::vector<GURL> urls_;
  const gfx::Rect bounds_;
  const ui::WindowShowState show_state_;
  const int32_t active_tab_index_;
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
    float x_offset) {
  return std::make_unique<HandleTabScrubbingAction>(x_offset);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::CreateBrowserWithRestoredData(
    const std::vector<GURL>& urls,
    const gfx::Rect& bounds,
    ui::WindowShowState show_state,
    int32_t active_tab_index,
    base::StringPiece app_name,
    int32_t restore_window_id) {
  return std::make_unique<CreateBrowserWithRestoredDataAction>(
      urls, bounds, show_state, active_tab_index, app_name, restore_window_id);
}

// No window will be opened in the following circumstances:
// 1. Lacros-chrome is initialized in the web Kiosk session
// 2. Full restore is responsible for restoring/launching Lacros.
// static
std::unique_ptr<BrowserAction> BrowserAction::GetActionForSessionStart() {
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest())
    return std::make_unique<NewWindowAction>(
        /*incognito=*/false, /*should_trigger_session_restore=*/false, -1);
  if (user_manager::UserManager::Get()->IsLoggedInAsWebKioskApp() ||
      ash::full_restore::MaybeCreateFullRestoreServiceForLacros())
    return std::make_unique<NoOpAction>();
  return std::make_unique<NewWindowAction>(
      /*incognito=*/false, /*should_trigger_session_restore=*/true, -1);
}

BrowserActionQueue::BrowserActionQueue() = default;
BrowserActionQueue::~BrowserActionQueue() = default;

bool BrowserActionQueue::IsEmpty() const {
  return actions_.empty();
}

void BrowserActionQueue::PushOrCancel(std::unique_ptr<BrowserAction> action) {
  if (action->IsQueueable()) {
    actions_.push(std::move(action));
  } else {
    action->Cancel(mojom::CreationResult::kBrowserNotRunning);
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
