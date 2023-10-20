// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_CAPTIVE_PORTAL_VIEW_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_CAPTIVE_PORTAL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/ui/simple_web_view_dialog.h"

namespace ash {
class CaptivePortalWindowProxy;

class CaptivePortalView : public SimpleWebViewDialog {
 public:
  CaptivePortalView(Profile* profile,
                    CaptivePortalWindowProxy* proxy,
                    const std::string& network_name);

  CaptivePortalView(const CaptivePortalView&) = delete;
  CaptivePortalView& operator=(const CaptivePortalView&) = delete;

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
  raw_ptr<CaptivePortalWindowProxy, DanglingUntriaged | ExperimentalAsh> proxy_;

  const std::string network_name_;
  bool redirected_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_CAPTIVE_PORTAL_VIEW_H_
