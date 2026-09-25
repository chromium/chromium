// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_WEB_CONTENTS_MAHI_TAB_HELPER_H_
#define CHROME_BROWSER_ASH_MAHI_WEB_CONTENTS_MAHI_TAB_HELPER_H_

#include <memory>

#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderWidgetHost;
class WebContents;
}  // namespace content

namespace mahi {

// Tab helper to observer the focused tab changes on browser side.
class MahiTabHelper : public content::WebContentsObserver {
 public:
  // Creates MahiTabHelper if mahi is enabled.
  static std::unique_ptr<MahiTabHelper> MaybeCreate(
      content::WebContents* web_contents);

  MahiTabHelper(const MahiTabHelper&) = delete;
  MahiTabHelper& operator=(const MahiTabHelper&) = delete;

  ~MahiTabHelper() override;

  // content::WebContentObserver:
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;

 private:
  explicit MahiTabHelper(content::WebContents* web_contents);

  // Boolean to indicate if this web contents get focused. Only one web content
  // can get focused at the same time.
  bool focused_ = false;
};

}  // namespace mahi

#endif  // CHROME_BROWSER_ASH_MAHI_WEB_CONTENTS_MAHI_TAB_HELPER_H_
