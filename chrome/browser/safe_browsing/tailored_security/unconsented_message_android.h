// Copyright 2021 The Chromium Authors
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
// them to enable Enhanced Safe Browsing when the Tailored Security preference
// changes.
class TailoredSecurityUnconsentedMessageAndroid {
 public:
  // Show the message for the given |web_contents| when the user |is_in_flow| of
  // doing the account-level opt-in. Calls |dismiss_callback| once the message
  // is complete.
  TailoredSecurityUnconsentedMessageAndroid(content::WebContents* web_contents,
                                            base::OnceClosure dismiss_callback,
                                            bool is_in_flow);
  ~TailoredSecurityUnconsentedMessageAndroid();

 private:
  void HandleMessageAccepted();
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);

  base::OnceClosure dismiss_callback_;
  raw_ptr<content::WebContents> web_contents_;
  bool is_in_flow_;
  std::unique_ptr<messages::MessageWrapper> message_;
  gfx::ImageSkia icon_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_UNCONSENTED_MESSAGE_ANDROID_H_
