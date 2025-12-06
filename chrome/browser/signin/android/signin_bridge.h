// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_BRIDGE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_BRIDGE_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_metrics.h"

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

// The glue for Java-side implementation of SigninBridge.
class SigninBridge : public KeyedService {
 public:
  SigninBridge() = default;
  ~SigninBridge() override = default;

  // Opens a add account flow pre-filled with |prefilled_email| that opens
  // the specified |continue_url| upon completion.
  virtual void StartAddAccountFlow(ui::WindowAndroid* window,
                                   const std::string& prefilled_email,
                                   const GURL& continue_url);

  // Opens the account management screen.
  virtual void OpenAccountManagementScreen(
      ui::WindowAndroid* window,
      signin::GAIAServiceType service_type);

  // Opens the account picker bottomsheet.
  virtual void OpenAccountPickerBottomSheet(content::WebContents* web_contents,
                                            const GURL& continue_url);
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_BRIDGE_H_
