// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_UNCONSENTED_MESSAGE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_UNCONSENTED_MESSAGE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class WebContents;
}  // namespace content

namespace safe_browsing {

// This class encapsulates the process of showing a message to a user prompting
// them to enable Enhacned Safe Browsing when the Tailored Security preference
// changes.
class TailoredSecurityUnconsentedModalAndroid {
 public:
  explicit TailoredSecurityUnconsentedModalAndroid(
      content::WebContents* web_contents);
  ~TailoredSecurityUnconsentedModalAndroid();

 private:
  void HandleMessageAccepted();
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);
  void HandleSettingsClicked();

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<messages::MessageWrapper> message_;
  gfx::ImageSkia icon_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_UNCONSENTED_MESSAGE_ANDROID_H_
