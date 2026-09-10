// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/glic_gemini_enterprise_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {

GlicGeminiEnterpriseManager::GlicGeminiEnterpriseManager(Profile* profile)
    : profile_(profile), gaia_origin_(GaiaUrls::GetInstance()->gaia_origin()) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  std::string guest_url_str =
      command_line->HasSwitch(::switches::kGlicGuestURL)
          ? command_line->GetSwitchValueASCII(::switches::kGlicGuestURL)
          : features::kGlicGuestURL.Get();
  if (!guest_url_str.empty()) {
    GURL guest_url(guest_url_str);
    if (guest_url.is_valid()) {
      guest_origin_ = url::Origin::Create(guest_url);
    }
  }
}

GlicGeminiEnterpriseManager::~GlicGeminiEnterpriseManager() = default;

void GlicGeminiEnterpriseManager::Bind(
    mojo::PendingReceiver<mojom::GeminiEnterpriseHandler> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

bool GlicGeminiEnterpriseManager::IsSignInURLAllowed(const GURL& url) const {
  // Reject invalid URLs, non-HTTPS schemes, URLs with embedded credentials,
  // or URLs exceeding maximum allowed length.
  constexpr size_t kMaxSignInUrlLength = 65536;
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme) ||
      url.has_username() || url.has_password() ||
      url.spec().length() > kMaxSignInUrlLength) {
    return false;
  }
  url::Origin origin = url::Origin::Create(url);
  if (origin == gaia_origin_) {
    return true;
  }
  if (guest_origin_.has_value() && origin == *guest_origin_) {
    return true;
  }
  return false;
}

BrowserWindowInterface*
GlicGeminiEnterpriseManager::GetLastActiveBrowserWindowForCurrentProfile()
    const {
  if (!profile_) {
    return nullptr;
  }
  BrowserWindowInterface* active_browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL &&
            browser->GetProfile() == profile_) {
          active_browser = browser;
          return false;
        }
        return true;
      });
  return active_browser;
}

void GlicGeminiEnterpriseManager::OpenSignInTab(
    mojom::OpenSignInTabOptionsPtr options,
    OpenSignInTabCallback callback) {
  if (!options || !options->signin_url.has_value() ||
      !options->signin_url.value().is_valid()) {
    std::move(callback).Run(mojom::OpenSignInTabResult::kErrorNoUrl);
    return;
  }
  const GURL& signin_url = options->signin_url.value();
  DVLOG(1) << "[glic_gemini_enterprise] OpenSignInTab: " << signin_url;
  if (!profile_ || profile_->IsOffTheRecord()) {
    std::move(callback).Run(mojom::OpenSignInTabResult::kErrorFailure);
    return;
  }
  if (!IsSignInURLAllowed(signin_url)) {
    DVLOG(1) << "[glic_gemini_enterprise] OpenSignInTab rejected disallowed "
                "sign-in URL: "
             << signin_url;
    std::move(callback).Run(mojom::OpenSignInTabResult::kErrorDisallowedUrl);
    return;
  }
  // If a sign-in tab is already open, activate it instead of opening a
  // duplicate.
  if (tabs::TabInterface* signin_tab = signin_tab_.Get()) {
    if (BrowserWindowInterface* browser =
            signin_tab->GetBrowserWindowInterface()) {
      if (auto* tab_list = TabListInterface::From(browser)) {
        tab_list->ActivateTab(signin_tab_);
        std::move(callback).Run(mojom::OpenSignInTabResult::kSuccess);
        return;
      }
    }
  }

  BrowserWindowInterface* browser_window =
      GetLastActiveBrowserWindowForCurrentProfile();
  if (!browser_window) {
    std::move(callback).Run(mojom::OpenSignInTabResult::kErrorFailure);
    return;
  }

  if (auto* tab_list = TabListInterface::From(browser_window)) {
    if (auto* active_tab = tab_list->GetActiveTab()) {
      tab_active_before_signin_ = active_tab->GetHandle();
    }
  }

  auto params = std::make_unique<NavigateParams>(
      profile_, signin_url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params->browser = browser_window;
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params->user_gesture = false;

  base::WeakPtr<content::NavigationHandle> handle =
      glic::Navigate(std::move(params));
  if (handle && handle->GetWebContents()) {
    if (tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(
            handle->GetWebContents())) {
      signin_tab_ = tab->GetHandle();
      has_opened_signin_tab_ = true;
      std::move(callback).Run(mojom::OpenSignInTabResult::kSuccess);
      return;
    }
  } else if (browser_window) {
    if (auto* tab_list = TabListInterface::From(browser_window)) {
      if (auto* active_tab = tab_list->GetActiveTab()) {
        // Only treat as the sign-in tab if a new tab was actually opened
        // (distinct from the tab that was active before sign-in).
        if (active_tab->GetHandle() != tab_active_before_signin_) {
          signin_tab_ = active_tab->GetHandle();
          has_opened_signin_tab_ = true;
          std::move(callback).Run(mojom::OpenSignInTabResult::kSuccess);
          return;
        }
      }
    }
  }
  std::move(callback).Run(mojom::OpenSignInTabResult::kErrorFailure);
}

void GlicGeminiEnterpriseManager::CloseSignInTab(
    mojom::CloseSignInTabOptionsPtr options,
    CloseSignInTabCallback callback) {
  DVLOG(1) << "[glic_gemini_enterprise] CloseSignInTab, signin_tab="
           << signin_tab_.Get()
           << ", has_opened_signin_tab=" << has_opened_signin_tab_;
  if (!has_opened_signin_tab_) {
    std::move(callback).Run(mojom::CloseSignInTabResult::kNoSignInTab);
    return;
  }

  has_opened_signin_tab_ = false;

  tabs::TabInterface* signin_tab = signin_tab_.Get();
  signin_tab_ = tabs::TabHandle();

  if (!signin_tab) {
    tab_active_before_signin_ = tabs::TabHandle();
    std::move(callback).Run(mojom::CloseSignInTabResult::kAlreadyClosed);
    return;
  }

  signin_tab->Close();

  if (tabs::TabInterface* orig = tab_active_before_signin_.Get()) {
    if (BrowserWindowInterface* browser = orig->GetBrowserWindowInterface()) {
      if (auto* tab_list = TabListInterface::From(browser)) {
        tab_list->ActivateTab(tab_active_before_signin_);
      }
    }
  }
  tab_active_before_signin_ = tabs::TabHandle();
  std::move(callback).Run(mojom::CloseSignInTabResult::kSuccess);
}

}  // namespace glic
