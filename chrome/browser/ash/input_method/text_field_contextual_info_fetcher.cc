// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace ash {
namespace input_method {

TextFieldContextualInfo::TextFieldContextualInfo() = default;

TextFieldContextualInfo::~TextFieldContextualInfo() = default;

void GetTextFieldAppTypeAndKey(TextFieldContextualInfo& info) {
  aura::Window* window = ash::window_util::GetActiveWindow();
  if (!window) {
    info.app_type = chromeos::AppType::NON_APP;
    return;
  }

  info.app_type = window->GetProperty(chromeos::kAppTypeKey);

  const std::string* key = window->GetProperty(ash::kAppIDKey);
  if (key) {
    info.app_key = *key;
  }
}

TextFieldContextualInfo GetTextFieldContextualInfo() {
  TextFieldContextualInfo info;
  GetTextFieldAppTypeAndKey(info);

  if (info.app_type == chromeos::AppType::BROWSER) {
    if (std::optional<GURL> url = GetUrlForTextFieldOnAshChrome();
        url.has_value()) {
      info.tab_url = *url;
    }
  }

  return info;
}

std::optional<GURL> GetUrlForTextFieldOnAshChrome() {
  ash::BrowserDelegate* browser =
      ash::BrowserController::GetInstance()->GetLastUsedBrowser();
  // Ash chrome will return true for browser->IsActive() if the
  // user is currently typing in an ash browser tab.
  if (browser && browser->IsActive() && browser->GetActiveWebContents()) {
    return browser->GetActiveWebContents()->GetLastCommittedURL();
  }

  return std::nullopt;
}

}  // namespace input_method
}  // namespace ash
