// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_TEXT_FIELD_CONTEXTUAL_INFO_FETCHER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_TEXT_FIELD_CONTEXTUAL_INFO_FETCHER_H_

#include <optional>
#include <string>

#include "chrome/browser/ui/browser_finder.h"
#include "chromeos/ui/base/app_types.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

struct TextFieldContextualInfo {
  TextFieldContextualInfo();
  ~TextFieldContextualInfo();

  // Type of app associated with the text field.
  chromeos::AppType app_type;
  // Optional, key of the app associated with this text field.
  std::string app_key;
  // Optional, tab's url where this text field is.
  GURL tab_url;
};

// Get the type and key of the current active app where the text filed is
// hosted. This is a lightweight and synced call.
void GetTextFieldAppTypeAndKey(TextFieldContextualInfo& info);

// Gets the contextual info of the currently focused text field.
TextFieldContextualInfo GetTextFieldContextualInfo();

// Get the current tab url if the text field is hosted by a tab.
std::optional<GURL> GetUrlForTextFieldOnAshChrome();

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_TEXT_FIELD_CONTEXTUAL_INFO_FETCHER_H_
