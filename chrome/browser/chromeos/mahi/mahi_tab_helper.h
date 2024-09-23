// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_TAB_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderWidgetHost;
class WebContents;
}  // namespace content

namespace mahi {

// Tab helper to observer the focused tab changes on browser side.
class MahiTabHelper : public content::WebContentsUserData<MahiTabHelper>,
                      public content::WebContentsObserver {
 public:
  // Creates MahiTabHelper and attaches it the `web_contents` if mahi is
  // enabled.
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  MahiTabHelper(const MahiTabHelper&) = delete;
  MahiTabHelper& operator=(const MahiTabHelper&) = delete;

  ~MahiTabHelper() override = default;

  // content::WebContentObserver:
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<MahiTabHelper>;

  // The only constructor is private because it will only be called by the
  // WebContentsUserData.
  explicit MahiTabHelper(content::WebContents* web_contents);

  // Boolean to indicate if this web contents get focused. Only one web content
  // can get focused at the same time.
  bool focused_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_TAB_HELPER_H_
