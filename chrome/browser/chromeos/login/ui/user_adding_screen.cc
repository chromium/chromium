// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen_input_methods_controller.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

namespace {

class UserAddingScreenImpl : public UserAddingScreen {
 public:
  void Start() override;
  void Cancel() override;
  bool IsRunning() override;

  void AddObserver(UserAddingScreen::Observer* observer) override;
  void RemoveObserver(UserAddingScreen::Observer* observer) override;

  static UserAddingScreenImpl* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<UserAddingScreenImpl>;
  class LoadTimeReporterWebUi : public OobeUI::Observer {
   public:
    LoadTimeReporterWebUi() : start_time_(base::TimeTicks::Now()) {}
    LoadTimeReporterWebUi(const LoadTimeReporterWebUi&) = delete;
    LoadTimeReporterWebUi& operator=(const LoadTimeReporterWebUi&) = delete;

    ~LoadTimeReporterWebUi() override {
      if (remove_observer_)
        oobe_ui_->RemoveObserver(this);
      remove_observer_ = false;
    }

    void Observe(OobeUI* oobe_ui) {
      oobe_ui_ = oobe_ui;
      oobe_ui_->AddObserver(this);
      remove_observer_ = true;
    }

   private:
    // OobeUI::Observer:
    void OnCurrentScreenChanged(OobeScreenId current_screen,
                                OobeScreenId new_screen) override {
      if (new_screen != OobeScreen::SCREEN_ACCOUNT_PICKER)
        return;
      const base::TimeDelta load_time = base::TimeTicks::Now() - start_time_;
      UmaHistogramTimes("ChromeOS.UserAddingScreen.LoadTime", load_time);
      remove_observer_ = false;
      oobe_ui_->RemoveObserver(this);
    }
    void OnDestroyingOobeUI() override { remove_observer_ = false; }

    const base::TimeTicks start_time_;
    OobeUI* oobe_ui_ = nullptr;
    bool remove_observer_ = false;
  };

  class LoadTimeReporterMojo : public LoginScreenShownObserver {
   public:
    LoadTimeReporterMojo() : start_time_(base::TimeTicks::Now()) {
      LoginScreenClient::Get()->AddLoginScreenShownObserver(this);
    }
    LoadTimeReporterMojo(const LoadTimeReporterMojo&) = delete;
    LoadTimeReporterMojo& operator=(const LoadTimeReporterMojo&) = delete;

    ~LoadTimeReporterMojo() override {
      // In tests, LoginScreenClient's instance may be destroyed before
      // LoadTimeReporterMojo's destructor is called.
      if (LoginScreenClient::HasInstance())
        LoginScreenClient::Get()->RemoveLoginScreenShownObserver(this);
    }

    // LoginScreenShownObserver:
    void OnLoginScreenShown() override {
      const base::TimeDelta load_time = base::TimeTicks::Now() - start_time_;
      UmaHistogramTimes("ChromeOS.UserAddingScreen.LoadTimeViewsBased",
                        load_time);
      LoginScreenClient::Get()->RemoveLoginScreenShownObserver(this);
    }

   private:
    const base::TimeTicks start_time_;
  };

  std::unique_ptr<LoadTimeReporterWebUi> reporter_web_ui_;
  std::unique_ptr<LoadTimeReporterMojo> reporter_mojo_;

  void OnDisplayHostCompletion();

  UserAddingScreenImpl();
  ~UserAddingScreenImpl() override;

  base::ObserverList<UserAddingScreen::Observer>::Unchecked observers_;
  LoginDisplayHost* display_host_;

  UserAddingScreenInputMethodsController im_controller_;
};

void UserAddingScreenImpl::Start() {
  CHECK(!IsRunning());
  bool viewBasedEnabled =
      chromeos::features::IsViewBasedMultiprofileLoginEnabled();
  if (viewBasedEnabled) {
    display_host_ = new chromeos::LoginDisplayHostMojo(
        LoginDisplayHostMojo::DisplayedScreen::USER_ADDING_SCREEN);
    reporter_mojo_ = std::make_unique<LoadTimeReporterMojo>();
  } else {
    display_host_ = new chromeos::LoginDisplayHostWebUI();
    reporter_web_ui_ = std::make_unique<LoadTimeReporterWebUi>();
  }
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  display_host_->StartUserAdding(base::BindOnce(
      &UserAddingScreenImpl::OnDisplayHostCompletion, base::Unretained(this)));
  if (!viewBasedEnabled) {
    reporter_web_ui_->Observe(display_host_->GetOobeUI());
  }

  for (auto& observer : observers_)
    observer.OnUserAddingStarted();
}

void UserAddingScreenImpl::Cancel() {
  CHECK(IsRunning());
  reporter_web_ui_.reset();
  reporter_mojo_.reset();

  display_host_->CancelUserAdding();

  // Reset wallpaper if cancel adding user from multiple user sign in page.
  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    WallpaperControllerClient::Get()->ShowUserWallpaper(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  }
}

bool UserAddingScreenImpl::IsRunning() {
  return display_host_ != NULL;
}

void UserAddingScreenImpl::AddObserver(UserAddingScreen::Observer* observer) {
  observers_.AddObserver(observer);
}

void UserAddingScreenImpl::RemoveObserver(
    UserAddingScreen::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserAddingScreenImpl::OnDisplayHostCompletion() {
  CHECK(IsRunning());
  display_host_ = NULL;

  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  for (auto& observer : observers_)
    observer.OnUserAddingFinished();
}

// static
UserAddingScreenImpl* UserAddingScreenImpl::GetInstance() {
  return base::Singleton<UserAddingScreenImpl>::get();
}

UserAddingScreenImpl::UserAddingScreenImpl()
    : display_host_(NULL), im_controller_(this) {}

UserAddingScreenImpl::~UserAddingScreenImpl() {}

}  // anonymous namespace

UserAddingScreen::UserAddingScreen() {}
UserAddingScreen::~UserAddingScreen() {}

UserAddingScreen* UserAddingScreen::Get() {
  return UserAddingScreenImpl::GetInstance();
}

}  // namespace chromeos
