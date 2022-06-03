// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_MODEL_DELEGATE_H_
#define CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_MODEL_DELEGATE_H_

#include "components/security_state/content/android/security_state_model_delegate.h"

namespace content {
class WebContents;
}  // namespace content

class ChromeSecurityStateModelDelegate : public SecurityStateModelDelegate {
 public:
  ChromeSecurityStateModelDelegate() = default;
  ~ChromeSecurityStateModelDelegate() override = default;

  // SecurityStateModelDelegate.
  security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) const override;
};

#endif  // CHROME_BROWSER_SSL_CHROME_SECURITY_STATE_MODEL_DELEGATE_H_
