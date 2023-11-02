// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OOM_INTERVENTION_NEAR_OOM_REDUCTION_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_OOM_INTERVENTION_NEAR_OOM_REDUCTION_MESSAGE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/interventions/intervention_delegate.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents.h"

namespace oom_intervention {

// A near OOM reduction delegate responsible for showing message bubbles.
// Created when high memory usage is detected and web page execution
// (JavaScript execution) is stopped to prevent OOM crashes.
class NearOomReductionMessageDelegate {
 public:
  NearOomReductionMessageDelegate();
  ~NearOomReductionMessageDelegate();

  void ShowMessage(content::WebContents* web_contents,
                   InterventionDelegate* intervention_delegate);
  void DismissMessage(messages::DismissReason dismiss_reason);

  messages::MessageWrapper* message_for_testing() { return message_.get(); }

 private:
  // Invoked when the user clicks on "Show" to decline the high memory usage
  // intervention and continue web page execution with original content.
  void HandleDeclineInterventionClicked();

  // Invoked when the message is dismissed, whether by the user, explicitly
  // in the code or automatically.
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);

  std::unique_ptr<messages::MessageWrapper> message_;
  raw_ptr<InterventionDelegate> intervention_delegate_;
};

}  // namespace oom_intervention

#endif  // CHROME_BROWSER_ANDROID_OOM_INTERVENTION_NEAR_OOM_REDUCTION_MESSAGE_DELEGATE_H_
