// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ACTION_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ACTION_H_

namespace ash::input_method {

enum class ConsentAction : int {
  // User explicitly hits "Yes/Agree" button.
  kApproved,
  // User dismisses the consent window.
  kDismissed,
  // User explicitly hits "No/Disagree" button.
  kDeclined
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_ACTION_H_
