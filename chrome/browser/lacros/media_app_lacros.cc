// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/media_app_lacros.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chromeos/lacros/lacros_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "ui/base/page_transition_types.h"

namespace crosapi {

MediaAppLacros::MediaAppLacros() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsSupported<mojom::MediaApp>()) {
    return;
  }
  service->BindMediaApp(receiver_.BindNewPipeAndPassRemote());
}

MediaAppLacros::~MediaAppLacros() = default;

void MediaAppLacros::SubmitForm(const GURL& url,
                                const std::vector<int8_t>& payload,
                                const std::string& header,
                                SubmitFormCallback callback) {
  // Keep this impl in sync with
  // chrome/browser/ash/system_web_apps/apps/media_app/chrome_media_app_ui_delegate.cc
  Profile* profile = ProfileManager::GetLastUsedProfile();
  NavigateParams navigate_params(
      profile, url,
      // The page transition is chosen to satisfy one of the conditions in
      // lacros_url_handling::IsNavigationInterceptable.
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_API |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;
  navigate_params.post_data = network::ResourceRequestBody::CreateFromBytes(
      reinterpret_cast<const char*>(payload.data()), payload.size());
  navigate_params.extra_headers = header;

  navigate_params.browser = chrome::FindTabbedBrowser(profile, false);
  if (!navigate_params.browser &&
      Browser::GetCreationStatusForProfile(profile) ==
          Browser::CreationStatus::kOk) {
    Browser::CreateParams create_params(profile, navigate_params.user_gesture);
    create_params.should_trigger_session_restore = false;
    navigate_params.browser = Browser::Create(create_params);
  }

  Navigate(&navigate_params);

  std::move(callback).Run();
}

}  // namespace crosapi
