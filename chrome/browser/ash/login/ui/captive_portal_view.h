// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_CAPTIVE_PORTAL_VIEW_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_CAPTIVE_PORTAL_VIEW_H_

#include <string>
#include "base/macros.h"
#include "chrome/browser/ash/login/ui/simple_web_view_dialog.h"

namespace chromeos {

class CaptivePortalWindowProxy;

class CaptivePortalView : public SimpleWebViewDialog {
 public:
  CaptivePortalView(Profile* profile, CaptivePortalWindowProxy* proxy);
  ~CaptivePortalView() override;

  // Starts loading.
  void StartLoad();

  // Overridden from content::WebContentsDelegate:
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;

  // Overridden from SimpleWebViewDialog:
  std::unique_ptr<views::WidgetDelegate> MakeWidgetDelegate() override;

 private:
  // Contains CaptivePortalWindowProxy to be notified when redirection state is
  // resolved.
  CaptivePortalWindowProxy* proxy_;

  bool redirected_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_CAPTIVE_PORTAL_VIEW_H_
