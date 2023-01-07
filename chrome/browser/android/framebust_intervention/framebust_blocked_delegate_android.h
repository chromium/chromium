// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FRAMEBUST_INTERVENTION_FRAMEBUST_BLOCKED_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_FRAMEBUST_INTERVENTION_FRAMEBUST_BLOCKED_DELEGATE_ANDROID_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class HostContentSettingsMap;

namespace content {
class WebContents;
}  // namespace content

namespace blocked_content {

// A framebust blocked delegate responsible for showing message bubbles.
// Created lazily when a framebust is first blocked, and matches the
// lifetime of WebContents afterwards.
class FramebustBlockedMessageDelegate
    : public content::WebContentsUserData<FramebustBlockedMessageDelegate> {
 public:
  // Describes the actions the user can take regarding this intervention, they
  // are provided through a callback the caller can pass to the delegate's
  // constructor.
  // This enum backs a histogram. Any updates should be reflected in enums.xml,
  // and new elements should only be appended to the end.
  enum class InterventionOutcome {
    kAccepted = 0,
    kDeclinedAndNavigated = 1,
    kMaxValue = kDeclinedAndNavigated
  };

  typedef base::OnceCallback<void(InterventionOutcome)> OutcomeCallback;

  bool ShowMessage(const GURL& blocked_url,
                   HostContentSettingsMap* settings_map,
                   OutcomeCallback intervention_callback);

  ~FramebustBlockedMessageDelegate() override;

  messages::MessageWrapper* message_for_testing() { return message_.get(); }

 private:
  friend class content::WebContentsUserData<FramebustBlockedMessageDelegate>;

  explicit FramebustBlockedMessageDelegate(content::WebContents* web_contents);
  void HandleClick();
  void HandleDismissCallback(messages::DismissReason dismiss_reason);
  void HandleOpenLink();

  raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;

  GURL url_;
  bool allow_settings_changes_ = false;
  OutcomeCallback intervention_callback_;
  std::unique_ptr<messages::MessageWrapper> message_;
  GURL blocked_url_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace blocked_content

#endif  // CHROME_BROWSER_ANDROID_FRAMEBUST_INTERVENTION_FRAMEBUST_BLOCKED_DELEGATE_ANDROID_H_
