// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_PASSWORD_REUSE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_PASSWORD_REUSE_CONTROLLER_ANDROID_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

namespace safe_browsing {

class PasswordReuseDialogViewAndroid;

// A self-owned class which manages |PasswordReuseDialogViewAndroid|. Created
// when password reuse modal is shown. Deleted when |CloseDialog| is called from
// Java side, or WebContents is destroyed, or password protection service is
// destroyed.
class PasswordReuseControllerAndroid
    : public ChromePasswordProtectionService::Observer,
      public content::WebContentsObserver {
 public:
  PasswordReuseControllerAndroid(content::WebContents* web_contents,
                                 ChromePasswordProtectionService* service,
                                 ReusedPasswordAccountType password_type,
                                 OnWarningDone done_callback);
  ~PasswordReuseControllerAndroid() override;

  // Called by |ChromePasswordProtectionService| when modal dialog needs to be
  // shown.
  void ShowDialog();
  // Called by |PasswordReuseDialogViewAndroid| when the dialog is closed from
  // Java side.
  void CloseDialog();

  // The following functions are called from |PasswordReuseDialogViewAndroid|,
  // to get text shown on the dialog.
  base::string16 GetButtonText() const;
  // Get the detailed warning text that should show in the modal warning dialog.
  // |placeholder_offsets| are the start points/indices of the placeholders that
  // are passed into the resource string.
  base::string16 GetWarningDetailText(
      std::vector<size_t>* placeholder_offsets) const;
  base::string16 GetTitle() const;
  const std::vector<base::string16> GetPlaceholdersForSavedPasswordWarningText()
      const;

  // ChromePasswordProtectionService::Observer:
  void OnGaiaPasswordChanged() override;
  void OnMarkingSiteAsLegitimate(const GURL& url) override;
  void InvokeActionForTesting(WarningAction action) override;
  WarningUIType GetObserverType() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  std::unique_ptr<PasswordReuseDialogViewAndroid> dialog_view_;
  ChromePasswordProtectionService* service_;
  const GURL url_;
  const ReusedPasswordAccountType password_type_;
  ui::WindowAndroid* window_android_;
  OnWarningDone done_callback_;

  // Records the start time when modal warning is constructed.
  base::TimeTicks modal_construction_start_time_;

  DISALLOW_COPY_AND_ASSIGN(PasswordReuseControllerAndroid);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_PASSWORD_REUSE_CONTROLLER_ANDROID_H_
