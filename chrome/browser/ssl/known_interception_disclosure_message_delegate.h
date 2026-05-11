// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_MESSAGE_DELEGATE_H_

#include <memory>

#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

// Message delegate to show a Known Interception Disclosure message on Android.
class KnownInterceptionDisclosureMessageDelegate
    : public content::WebContentsUserData<
          KnownInterceptionDisclosureMessageDelegate> {
 public:
  ~KnownInterceptionDisclosureMessageDelegate() override;

  // Shows the message if not already shown.
  void MaybeShow();

 private:
  friend class content::WebContentsUserData<
      KnownInterceptionDisclosureMessageDelegate>;
  friend class KnownInterceptionDisclosureMessageDelegateTest;

  explicit KnownInterceptionDisclosureMessageDelegate(
      content::WebContents* web_contents);

  void HandlePrimaryActionClick();
  void HandleDismissCallback(messages::DismissReason dismiss_reason);

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<messages::MessageWrapper> message_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_MESSAGE_DELEGATE_H_
