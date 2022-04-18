// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_OS_FEEDBACK_DELEGATE_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_OS_FEEDBACK_DELEGATE_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace ash {

// A delegate which exposes browser functionality from //chrome to the OS
// Feedback UI.
class OsFeedbackDelegate {
 public:
  virtual ~OsFeedbackDelegate() = default;

  // Gets the application locale so that suggested help contents can display
  // localized titles when available.
  virtual std::string GetApplicationLocale() = 0;
  // Returns the last active page url before the feedback tool is opened if any.
  virtual absl::optional<GURL> GetLastActivePageUrl() = 0;
  // Returns the normalized email address of the signed-in user associated with
  // the browser context, if any.
  virtual absl::optional<std::string> GetSignedInUserEmail() const = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_OS_FEEDBACK_DELEGATE_H_
