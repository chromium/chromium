// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

namespace ash {
namespace input_method {

namespace {

void TextFieldContextualInfoWithUrl(TextFieldContextualInfoCallback cb,
                                    TextFieldContextualInfo& info,
                                    const std::optional<GURL>& url) {
  if (url.has_value()) {
    info.tab_url = url.value();
  }
  std::move(cb).Run(std::move(info));
}

}  // namespace

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

void GetTextFieldContextualInfo(TextFieldContextualInfoCallback cb) {
  TextFieldContextualInfo info;
  GetTextFieldAppTypeAndKey(info);

  TextFieldContextualInfoWithUrl(std::move(cb), info,
                                 info.app_type == chromeos::AppType::BROWSER
                                     ? GetUrlForTextFieldOnAshChrome()
                                     : std::nullopt);
}

std::optional<GURL> GetUrlForTextFieldOnAshChrome() {
  Browser* browser = chrome::FindLastActive();
  // Ash chrome will return true for browser->window()->IsActive() if the
  // user is currently typing in an ash browser tab.
  if (browser && browser->window() && browser->window()->IsActive() &&
      browser->tab_strip_model() &&
      browser->tab_strip_model()->GetActiveWebContents()) {
    return browser->tab_strip_model()
        ->GetActiveWebContents()
        ->GetLastCommittedURL();
  }

  return std::nullopt;
}

}  // namespace input_method
}  // namespace ash
