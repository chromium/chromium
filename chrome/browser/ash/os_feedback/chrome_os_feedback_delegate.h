// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_
#define CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_

#include <string>

#include "ash/webui/os_feedback_ui/backend/os_feedback_delegate.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace ash {

class ChromeOsFeedbackDelegate : public OsFeedbackDelegate {
 public:
  explicit ChromeOsFeedbackDelegate(Profile* profile);
  ~ChromeOsFeedbackDelegate() override;

  ChromeOsFeedbackDelegate(const ChromeOsFeedbackDelegate&) = delete;
  ChromeOsFeedbackDelegate& operator=(const ChromeOsFeedbackDelegate&) = delete;

  // OsFeedbackDelegate:
  std::string GetApplicationLocale() override;
  absl::optional<GURL> GetLastActivePageUrl() override;
  absl::optional<std::string> GetSignedInUserEmail() const override;

 private:
  // TODO(xiangdongkong): make sure the profile_ cannot be destroyed while
  // operations are pending.
  raw_ptr<Profile> profile_;
  absl::optional<GURL> page_url_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OS_FEEDBACK_CHROME_OS_FEEDBACK_DELEGATE_H_
