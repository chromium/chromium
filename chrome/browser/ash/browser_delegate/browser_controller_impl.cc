// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"

#include <memory>
#include <unordered_map>

#include "base/check.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate_impl.h"
#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "chrome/browser/ash/browser_delegate/browser_type_conversion.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/window.h"

namespace {

bool BrowserMatchesURL(Browser* browser, const GURL& url) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetVisibleURL()
      .EqualsIgnoringRef(url);
}

bool BrowserMatches(Browser* browser,
                    Profile* profile,
                    webapps::AppId app_id,
                    Browser::Type type,
                    const GURL& url) {
  return browser->profile() == profile && browser->type() == type &&
         web_app::GetAppIdFromApplicationName(browser->app_name()) == app_id &&
         (url.is_empty() || BrowserMatchesURL(browser, url));
}

}  // namespace

namespace ash {

BrowserControllerImpl::BrowserControllerImpl() {
  observation_.Observe(BrowserList::GetInstance());
}

BrowserControllerImpl::~BrowserControllerImpl() = default;

BrowserDelegate* BrowserControllerImpl::GetDelegate(Browser* browser) {
  if (browser == nullptr) {
    return nullptr;
  }

  auto it = browsers_.find(browser);
  if (it == browsers_.end()) {
    it = browsers_
             .insert({browser, std::make_unique<BrowserDelegateImpl>(browser)})
             .first;
  }
  return it->second.get();
}

BrowserDelegate* BrowserControllerImpl::GetLastUsedVisibleBrowser() {
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (browser->window()->GetNativeWindow()->IsVisible()) {
      return GetDelegate(browser);
    }
  }
  return nullptr;
}

BrowserDelegate* BrowserControllerImpl::GetLastUsedVisibleOnTheRecordBrowser() {
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (!browser->profile()->IsOffTheRecord() &&
        browser->window()->IsVisible()) {
      return GetDelegate(browser);
    }
  }
  return nullptr;
}

BrowserDelegate* BrowserControllerImpl::FindWebApp(
    const user_manager::User& user,
    webapps::AppId app_id,
    BrowserType browser_type,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByUser(&user));
  CHECK(profile);

  CHECK(browser_type == BrowserType::kApp ||
        browser_type == BrowserType::kAppPopup);
  Browser::Type internal_type = ToInternalBrowserType(browser_type);

  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (!browser->is_delete_scheduled() &&
        BrowserMatches(browser, profile, app_id, internal_type, url)) {
      return GetDelegate(browser);
    }
  }

  return nullptr;
}

BrowserDelegate* BrowserControllerImpl::NewTabWithPostData(
    const user_manager::User& user,
    const GURL& url,
    base::span<const uint8_t> post_data,
    std::string_view extra_headers) {
  Profile* profile = Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByUser(&user));
  CHECK(profile);

  NavigateParams navigate_params(
      profile, url,
      // TODO(crbug.com/369688254): The page transition was chosen to satisfy
      // some obsolete condition and should be revisited.
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_API |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;
  navigate_params.post_data =
      network::ResourceRequestBody::CreateFromCopyOfBytes(post_data);
  navigate_params.extra_headers = std::string(extra_headers);

  navigate_params.browser = chrome::FindTabbedBrowser(profile, false);
  if (!navigate_params.browser &&
      Browser::GetCreationStatusForProfile(profile) ==
          Browser::CreationStatus::kOk) {
    Browser::CreateParams create_params(profile, navigate_params.user_gesture);
    create_params.should_trigger_session_restore = false;
    navigate_params.browser = Browser::Create(create_params);
  }

  Navigate(&navigate_params);
  return GetDelegate(navigate_params.browser);
}

BrowserDelegate* BrowserControllerImpl::CreateWebApp(
    const user_manager::User& user,
    webapps::AppId app_id,
    BrowserType browser_type,
    const CreateParams& params) {
  CHECK(browser_type == BrowserType::kApp ||
        browser_type == BrowserType::kAppPopup)
      << "Unexpected BrowserType: " << static_cast<int>(browser_type);
  const bool popup = browser_type == BrowserType::kAppPopup;

  Profile* profile = Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByUser(&user));
  CHECK(profile);

  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    LOG(WARNING) << "Cannot create browser for given profile";
    return nullptr;
  }

  Browser::CreateParams cparams =
      web_app::CreateParamsForApp(app_id, popup,
                                  /*trusted_source=*/true,
                                  /*window_bounds=*/gfx::Rect(), profile,
                                  /*user_gesture=*/true);
  cparams.restore_id = params.restore_id;
  cparams.omit_from_session_restore = true;
  cparams.initial_show_state = ui::mojom::WindowShowState::kDefault;
  cparams.can_resize = params.allow_resize;
  cparams.can_maximize = params.allow_maximize;
  cparams.can_fullscreen = params.allow_fullscreen;
  return GetDelegate(
      web_app::CreateWebAppWindowMaybeWithHomeTab(app_id, cparams));
}

BrowserDelegate* BrowserControllerImpl::CreateCustomTab(
    const user_manager::User& user,
    std::unique_ptr<content::WebContents> contents) {
  Profile* profile = Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetBrowserContextByUser(&user));
  CHECK(profile);

  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return nullptr;
  }

  Browser::CreateParams params(Browser::TYPE_CUSTOM_TAB, profile,
                               /*user_gesture=*/true);
  params.omit_from_session_restore = true;
  Browser* browser = Browser::Create(params);
  browser->tab_strip_model()->AppendWebContents(std::move(contents),
                                                /*foreground=*/true);
  return GetDelegate(browser);
}

void BrowserControllerImpl::OnBrowserRemoved(Browser* browser) {
  browsers_.erase(browser);
  // The corresponding BrowserDelegateImpl, if any, is now dead.
}

}  // namespace ash
