// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

namespace password_manager {
struct CredentialUIEntry;
}

namespace glic {
class GlicKeyedService;
}

// Handles a password change flow leveraging Gemini in Chrome.
// This flow is initiated for a specific credential from the Password Checkup
// page.
class PasswordChangeFromCheckupDelegate {
 public:
  PasswordChangeFromCheckupDelegate();
  ~PasswordChangeFromCheckupDelegate();

  void StartPasswordChangeFlow(
      const password_manager::CredentialUIEntry& credential,
      base::WeakPtr<content::WebContents> web_contents);

 private:
  glic::GlicKeyedService* GetGlicService();

  base::WeakPtr<content::WebContents> originator_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_FROM_CHECKUP_DELEGATE_H_
