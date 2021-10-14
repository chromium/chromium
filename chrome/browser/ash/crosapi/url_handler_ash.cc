// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/url_handler_ash.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_url_window_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/display/types/display_constants.h"

namespace crosapi {

UrlHandlerAsh::UrlHandlerAsh() = default;
UrlHandlerAsh::~UrlHandlerAsh() {
  // It is assumed that url_window_manager_ outlives url_window_observer_.
  url_window_observer_.reset();
  url_window_manager_.reset();
}

void UrlHandlerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::UrlHandler> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void UrlHandlerAsh::OpenUrl(const GURL& url) {
  // Settings will be handled.
  if (url.DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUIOSSettingsURL).DeprecatedGetOriginAsURL()) {
    chrome::SettingsWindowManager* settings_window_manager =
        chrome::SettingsWindowManager::GetInstance();
    settings_window_manager->ShowChromePageForProfile(
        ProfileManager::GetPrimaryUserProfile(), url,
        display::kInvalidDisplayId);
    return;
  }

  // TODO(crbug/1256481): Only accept URL's from the Ash supplied allow list.
  if (url.DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUIFlagsURL).DeprecatedGetOriginAsURL()) {
    if (!url_window_manager_) {
      url_window_manager_ = std::make_unique<ChromeUrlWindowManager>();
      url_window_observer_ =
          std::make_unique<ChromeUrlWindowObserver>(url_window_manager_.get());
    }

    url_window_manager_->ShowChromePageForProfile(
        ProfileManager::GetPrimaryUserProfile(), url,
        display::kInvalidDisplayId);
    return;
  }
}

}  // namespace crosapi
