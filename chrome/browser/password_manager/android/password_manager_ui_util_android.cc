// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_ui_util_android.h"

#include "components/password_manager/content/browser/content_password_manager_driver.h"

using autofill::mojom::FocusedFieldType;

bool ShouldAcceptFocusEvent(
    content::WebContents* web_contents,
    password_manager::ContentPasswordManagerDriver* driver,
    FocusedFieldType focused_field_type) {
  // Only react to focus events that are sent for the current focused frame.
  // This is used to make sure that obsolete events that come in an unexpected
  // order are not processed. Example: (Frame1, focus) -> (Frame2, focus) ->
  // (Frame1, unfocus) would otherwise unset all the data set for Frame2, which
  // would be wrong.
  if (web_contents->GetFocusedFrame() &&
      driver->render_frame_host() == web_contents->GetFocusedFrame()) {
    return true;
  }

  // The only event that is accepted even if there is no focused frame is an
  // "unfocus" event that resulted in all frames being unfocused. This can be
  // used to reset the focused state.
  if (!web_contents->GetFocusedFrame() &&
      focused_field_type == FocusedFieldType::kUnknown) {
    return true;
  }
  return false;
}
