// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/url_handler_ash.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/display/types/display_constants.h"

namespace crosapi {

UrlHandlerAsh::UrlHandlerAsh() = default;
UrlHandlerAsh::~UrlHandlerAsh() = default;

void UrlHandlerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::UrlHandler> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void UrlHandlerAsh::OpenUrl(const GURL& url) {
  // For now, we only know how to handle the os-settings URL.
  if (url.GetOrigin() != GURL(chrome::kChromeUIOSSettingsURL).GetOrigin()) {
    LOG(ERROR) << "Unknown URL: " << url;
    return;
  }

  chrome::SettingsWindowManager* settings_window_manager =
      chrome::SettingsWindowManager::GetInstance();
  settings_window_manager->ShowChromePageForProfile(
      ProfileManager::GetPrimaryUserProfile(), url, display::kInvalidDisplayId);
}

}  // namespace crosapi
