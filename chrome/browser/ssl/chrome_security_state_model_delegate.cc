// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_security_state_model_delegate.h"

#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"

security_state::SecurityLevel
ChromeSecurityStateModelDelegate::GetSecurityLevel(
    content::WebContents* web_contents) const {
  DCHECK(web_contents);
  return chrome_security_state::GetSecurityLevel(web_contents);
}

security_state::MaliciousContentStatus
ChromeSecurityStateModelDelegate::GetMaliciousContentStatus(
    content::WebContents* web_contents) const {
  DCHECK(web_contents);
  return chrome_security_state::GetMaliciousContentStatus(web_contents);
}
