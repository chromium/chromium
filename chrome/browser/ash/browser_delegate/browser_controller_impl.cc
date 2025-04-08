// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"

#include <memory>
#include <unordered_map>

#include "base/check.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

BrowserControllerImpl::BrowserControllerImpl() {
  observation_.Observe(BrowserList::GetInstance());
}

BrowserControllerImpl::~BrowserControllerImpl() = default;

BrowserDelegate* BrowserControllerImpl::NewTabWithPostData(
    user_manager::User& user,
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
  return GetBrowserDelegate(navigate_params.browser);
}

BrowserDelegate* BrowserControllerImpl::GetBrowserDelegate(Browser* browser) {
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

void BrowserControllerImpl::OnBrowserRemoved(Browser* browser) {
  browsers_.erase(browser);
  // The corresponding BrowserDelegateImpl, if any, is now dead.
}

}  // namespace ash
