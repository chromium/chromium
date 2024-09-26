// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_action.h"

#include <optional>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/desk_template_ash.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/app_restore/features.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace crosapi {

namespace {

// Returns true if FullRestoreService can be created to restore/launch Lacros
// during the system startup phase when all of the below conditions are met:
// 1. The FullRestoreForLacros flag is enabled.
// 2. Lacros is enabled.
// 3. FullRestoreService can be created for the primary profile.
bool MaybeCreateFullRestoreServiceForLacros() {
  // Full restore for Lacros depends on BrowserAppInstanceRegistry to save and
  // restore Lacros windows, so check the web apps crosapi flag to make sure
  // BrowserAppInstanceRegistry is created.
  if (!::full_restore::features::IsFullRestoreForLacrosEnabled() ||
      !web_app::IsWebAppsCrosapiEnabled()) {
    return false;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(user);
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);

  // Lacros can be launched at the very early stage during the system startup
  // phase. So create FullRestoreService to construct LacrosWindowHandler to
  // observe BrowserAppInstanceRegistry for Lacros windows before the first
  // Lacros window is created, to avoid missing any Lacros windows.
  return ash::full_restore::FullRestoreServiceFactory::GetForProfile(profile);
}

}  // namespace

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
                  int64_t target_display_id,
                  std::optional<uint64_t> profile_id = std::nullopt)
      : BrowserAction(true),
        incognito_(incognito),
        should_trigger_session_restore_(should_trigger_session_restore),
        target_display_id_(target_display_id),
        profile_id_(profile_id),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    CHECK_GE(service.interface_version,
             crosapi::mojom::BrowserService::kNewWindowMinVersion);
    if (incognito_) {
      Profile* profile = ProfileManager::GetPrimaryUserProfile();
      if (!profile || !IncognitoModePrefs::IsIncognitoAllowed(profile))
        return;
    }
    service.service->NewWindow(incognito_, should_trigger_session_restore_,
                               target_display_id_, profile_id_,
                               base::BindOnce(&NewWindowAction::OnPerformed,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              std::move(on_performed)));
  }

 private:
  const bool incognito_;
  const bool should_trigger_session_restore_;
  const int64_t target_display_id_;
  const std::optional<uint64_t> profile_id_;
  base::WeakPtrFactory<NewWindowAction> weak_ptr_factory_;
};

class NewWindowForDetachingTabAction final : public BrowserAction {
 public:
  NewWindowForDetachingTabAction(std::u16string_view tab_id_str,
                                 std::u16string_view group_id_str,
                                 NewWindowForDetachingTabCallback callback)
      : BrowserAction(false),
        tab_id_str_(tab_id_str),
        group_id_str_(group_id_str),
        callback_(std::move(callback)),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    CHECK_GE(service.interface_version,
             mojom::BrowserService::kNewWindowForDetachingTabMinVersion);
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
  explicit NewTabAction(std::optional<uint64_t> profile_id = std::nullopt)
      : BrowserAction(true), profile_id_(profile_id), weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    CHECK_GE(service.interface_version,
             mojom::BrowserService::kNewTabMinVersion);
    service.service->NewTab(profile_id_,
                            base::BindOnce(&NewTabAction::OnPerformed,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(on_performed)));
  }

 private:
  std::optional<uint64_t> profile_id_;
  base::WeakPtrFactory<NewTabAction> weak_ptr_factory_;
};

class LaunchAction final : public BrowserAction {
 public:
  explicit LaunchAction(int64_t target_display_id,
                        std::optional<uint64_t> profile_id = std::nullopt)
      : BrowserAction(true),
        target_display_id_(target_display_id),
        profile_id_(profile_id),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    CHECK_GE(service.interface_version,
             mojom::BrowserService::kLaunchMinVersion);
    service.service->Launch(target_display_id_, profile_id_,
                            base::BindOnce(&LaunchAction::OnPerformed,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           std::move(on_performed)));
  }

 private:
  int64_t target_display_id_;
  std::optional<uint64_t> profile_id_;
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
    CHECK_GE(service.interface_version,
             mojom::BrowserService::kOpenUrlMinVersion);
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

class OpenCaptivePortalSigninAction final : public BrowserAction {
 public:
  explicit OpenCaptivePortalSigninAction(const GURL& url)
      : BrowserAction(true), url_(url), weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    if (service.interface_version <
        mojom::BrowserService::kOpenCaptivePortalSigninMinVersion) {
      LOG(ERROR) << "BrowserService does not support OpenCaptivePortalSignin";
      return;
    }
    service.service->OpenCaptivePortalSignin(
        url_, base::BindOnce(&OpenCaptivePortalSigninAction::OnPerformed,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(on_performed)));
  }

 private:
  const GURL url_;
  base::WeakPtrFactory<OpenCaptivePortalSigninAction> weak_ptr_factory_;
};

class NewGuestWindowAction final : public BrowserAction {
 public:
  explicit NewGuestWindowAction(int64_t target_display_id)
      : BrowserAction(true),
        target_display_id_(target_display_id),
        weak_ptr_factory_(this) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    CHECK_GE(service.interface_version,
             crosapi::mojom::BrowserService::kNewGuestWindowMinVersion);
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
    CHECK_GE(service.interface_version,
             crosapi::mojom::BrowserService::kHandleTabScrubbingMinVersion);
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
    CHECK_GE(service.interface_version,
             crosapi::mojom::BrowserService::kNewFullscreenWindowMinVersion);
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
    CHECK_GE(service.interface_version,
             crosapi::mojom::BrowserService::kRestoreTabMinVersion);
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
    CHECK_GE(service.interface_version,
             crosapi::mojom::BrowserService::kOpenForFullRestoreMinVersion);
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
      ui::mojom::WindowShowState show_state,
      int32_t active_tab_index,
      int32_t first_non_pinned_tab_index,
      std::string_view app_name,
      int32_t restore_window_id,
      uint64_t lacros_profile_id)
      : BrowserAction(true),
        urls_(urls),
        bounds_(bounds),
        tab_group_infos_(tab_group_infos),
        show_state_(show_state),
        active_tab_index_(active_tab_index),
        first_non_pinned_tab_index_(first_non_pinned_tab_index),
        app_name_(app_name),
        restore_window_id_(restore_window_id),
        lacros_profile_id_(lacros_profile_id) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    crosapi::mojom::DeskTemplateStatePtr additional_state =
        crosapi::mojom::DeskTemplateState::New(
            urls_, active_tab_index_, app_name_, restore_window_id_,
            first_non_pinned_tab_index_, tab_group_infos_, lacros_profile_id_);
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
  const ui::mojom::WindowShowState show_state_;
  const int32_t active_tab_index_;
  const int32_t first_non_pinned_tab_index_;
  const std::string app_name_;
  const int32_t restore_window_id_;
  const uint64_t lacros_profile_id_;
};

class OpenProfileManagerAction final : public BrowserAction {
 public:
  OpenProfileManagerAction() : BrowserAction(true) {}

  void Perform(const VersionedBrowserService& service,
               BrowserManagerCallback on_performed) override {
    CHECK_GE(service.interface_version,
             crosapi::mojom::BrowserService::kOpenProfileManagerMinVersion);
    service.service->OpenProfileManager();
  }
};

// static
std::unique_ptr<BrowserAction> BrowserAction::NewWindow(
    bool incognito,
    bool should_trigger_session_restore,
    int64_t target_display_id,
    std::optional<uint64_t> profile_id) {
  return std::make_unique<NewWindowAction>(
      incognito, should_trigger_session_restore, target_display_id, profile_id);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::NewTab(
    std::optional<uint64_t> profile_id) {
  return std::make_unique<NewTabAction>(profile_id);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::Launch(
    int64_t target_display_id,
    std::optional<uint64_t> profile_id) {
  return std::make_unique<LaunchAction>(target_display_id, profile_id);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::NewWindowForDetachingTab(
    std::u16string_view tab_id_str,
    std::u16string_view group_id_str,
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
std::unique_ptr<BrowserAction> BrowserAction::OpenCaptivePortalSignin(
    const GURL& url) {
  return std::make_unique<OpenCaptivePortalSigninAction>(url);
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
    ui::mojom::WindowShowState show_state,
    int32_t active_tab_index,
    int32_t first_non_pinned_tab_index,
    std::string_view app_name,
    int32_t restore_window_id,
    uint64_t lacros_profile_id) {
  return std::make_unique<CreateBrowserWithRestoredDataAction>(
      urls, bounds, tab_groups, show_state, active_tab_index,
      first_non_pinned_tab_index, app_name, restore_window_id,
      lacros_profile_id);
}

// static
std::unique_ptr<BrowserAction> BrowserAction::OpenProfileManager() {
  return std::make_unique<OpenProfileManagerAction>();
}

// No window will be opened in the following circumstances:
// 1. Lacros-chrome is initialized in the Kiosk session.
// 2. Full restore is responsible for restoring/launching Lacros.
// 3. Floating Workspace Service is responsible for restoring/launching lacros.
// static
std::unique_ptr<BrowserAction> BrowserAction::GetActionForSessionStart() {
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
    return std::make_unique<NewWindowAction>(
        /*incognito=*/false, /*should_trigger_session_restore=*/false, -1);
  }
  if (chromeos::IsKioskSession() ||
      ash::floating_workspace_util::ShouldHandleRestartRestore() ||
      MaybeCreateFullRestoreServiceForLacros()) {
    return std::make_unique<NoOpAction>();
  }
  return std::make_unique<NewWindowAction>(
      /*incognito=*/false, /*should_trigger_session_restore=*/true, -1);
}


}  // namespace crosapi
