// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_MESSAGE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_MESSAGE_DELEGATE_ANDROID_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/lookalikes/safety_tip_ui.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

// Message delegate to show a safety tip message on Android.
class SafetyTipMessageDelegateAndroid {
 public:
  SafetyTipMessageDelegateAndroid();
  ~SafetyTipMessageDelegateAndroid();

  void DisplaySafetyTipPrompt(
      security_state::SafetyTipStatus safety_tip_status,
      const GURL& suggested_url,
      content::WebContents* web_contents,
      base::OnceCallback<void(SafetyTipInteraction)> close_callback);

 private:
  friend class SafetyTipMessageDelegateAndroidTest;

  void HandleLeaveSiteClick();
  void HandleLearnMoreClick();

  void HandleDismissCallback(messages::DismissReason dismiss_reason);
  void DismissInternal();

  std::unique_ptr<messages::MessageWrapper> message_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  security_state::SafetyTipStatus safety_tip_status_;

  // The URL of the page the Safety Tip suggests you intended to go to, when
  // applicable (for SafetyTipStatus::kLookalike).
  GURL suggested_url_;

  SafetyTipInteraction action_taken_ = SafetyTipInteraction::kNoAction;
  base::OnceCallback<void(SafetyTipInteraction)> close_callback_;
};

#endif  // CHROME_BROWSER_LOOKALIKES_SAFETY_TIP_MESSAGE_DELEGATE_ANDROID_H_
