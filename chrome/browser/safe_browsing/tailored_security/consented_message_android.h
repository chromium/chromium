// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CONSENTED_MESSAGE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CONSENTED_MESSAGE_ANDROID_H_

#include <memory>

#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"

namespace content {
class WebContents;
}  // namespace content

namespace safe_browsing {

class TailoredSecurityConsentedModalAndroid {
 public:
  TailoredSecurityConsentedModalAndroid();
  ~TailoredSecurityConsentedModalAndroid();

  void DisplayMessage(content::WebContents* web_contents);
  void DismissMessage(messages::DismissReason dismiss_reason);

 private:
  void HandleSettingsClicked();
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);

  std::unique_ptr<messages::MessageWrapper> message_;
  content::WebContents* web_contents_ = nullptr;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_CONSENTED_MESSAGE_ANDROID_H_
