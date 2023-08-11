// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/net/browser_url_opener_impl.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/url_handler_ash.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"

namespace arc {

void BrowserUrlOpenerImpl::OpenUrl(GURL url) {
  if (crosapi::browser_util::IsLacrosEnabled() &&
      ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(url)) {
    crosapi::UrlHandlerAsh().OpenUrl(url);
  } else {
    ash::NewWindowDelegate::GetPrimary()->OpenUrl(
        url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
        ash::NewWindowDelegate::Disposition::kNewForegroundTab);
  }
}

}  // namespace arc
