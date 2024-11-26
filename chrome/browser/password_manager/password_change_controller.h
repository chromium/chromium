// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CONTROLLER_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// This class controls password change process. Password change process starts
// immediately after creating the object.
class PasswordChangeController
    : public password_manager::PasswordFormManagerObserver {
 public:
  PasswordChangeController(GURL change_password_url,
                           std::u16string username,
                           std::u16string password,
                           content::WebContents* originator);
  ~PasswordChangeController() override;

  PasswordChangeController(const PasswordChangeController&) = delete;
  PasswordChangeController& operator=(const PasswordChangeController&) = delete;

 private:
  // password_manager::PasswordFormManagerObserver Impl
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override;

  const GURL change_password_url_;
  const std::u16string username_;
  const std::u16string original_password_;

  base::WeakPtr<content::WebContents> originator_;
  base::WeakPtr<content::WebContents> executor_;

  base::WeakPtrFactory<PasswordChangeController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_CONTROLLER_H_
