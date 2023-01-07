// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_TEXT_FIELD_CONTEXTUAL_INFO_FETCHER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_TEXT_FIELD_CONTEXTUAL_INFO_FETCHER_H_

#include <string>

#include "ash/constants/app_types.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

struct TextFieldContextualInfo {
  TextFieldContextualInfo();
  ~TextFieldContextualInfo();

  // Type of app associated with the text field.
  ash::AppType app_type;
  // Optional, key of the app associated with this text field.
  std::string app_key;
  // Optional, tab's url where this text field is.
  GURL tab_url;
};

// Get the type and key of the current active app where the text filed is
// hosted. This is a lightweight and synced call.
void GetTextFieldAppTypeAndKey(TextFieldContextualInfo& info);

using TextFieldContextualInfoCallback =
    base::OnceCallback<void(const TextFieldContextualInfo& info)>;
// Get the contextual info of the current text filed.
// Its sub queries may go over IPCs.
void GetTextFieldContextualInfo(TextFieldContextualInfoCallback cb);

// Get the current tab url if the text field is hosted by a tab.
absl::optional<GURL> GetUrlForTextFieldOnAshChrome();

using TextFieldTabUrlCallback =
    base::OnceCallback<void(const absl::optional<GURL>& url)>;
// Get the current tab url if the text field is hosted by a tab from Lacros.
// This query requires a further call over IPC.
void GetUrlForTextFieldOnLacros(TextFieldTabUrlCallback cb);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_TEXT_FIELD_CONTEXTUAL_INFO_FETCHER_H_
