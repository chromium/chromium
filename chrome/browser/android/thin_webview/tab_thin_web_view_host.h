// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_THIN_WEBVIEW_TAB_THIN_WEB_VIEW_HOST_H_
#define CHROME_BROWSER_ANDROID_THIN_WEBVIEW_TAB_THIN_WEB_VIEW_HOST_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/android/thin_webview/jni_headers/ThinWebViewHost_shared_jni.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {
class NavigationHandle;
class WebContents;
struct OpenURLParams;
}  // namespace content

namespace thin_webview::android {

// Manages hosting a WebContents inside a tab-scoped container using
// ThinWebView.
//
// The hosted WebContents is tab-scoped: it survives the tab moving between
// Activities (tab reparenting). The Android View that displays it is
// Activity-scoped, so it is destroyed as soon as the tab leaves its Activity
// and lazily recreated by GetView() once the tab is in its new Activity.
class TabThinWebViewHost : public content::WebContentsDelegate {
 public:
  explicit TabThinWebViewHost(tabs::TabInterface& tab);
  ~TabThinWebViewHost() override;

  TabThinWebViewHost(const TabThinWebViewHost&) = delete;
  TabThinWebViewHost& operator=(const TabThinWebViewHost&) = delete;

  // Returns the root Android View of the ThinWebView to be displayed, creating
  // it if necessary. Returns null if there is nothing to display, e.g. when no
  // WebContents has been set, or when the tab is between Activities.
  jni_zero::ScopedJavaLocalRef<JView> GetView();

  // Sets or clears the WebContents to be displayed in the ThinWebView.
  void SetWebContents(content::WebContents* web_contents);

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;

  bool HasViewForTesting() const { return !j_host_.is_null(); }

 private:
  // Destroys the Java-side host, and with it the ThinWebView.
  void DestroyView();

  // tabs::TabInterface callback:
  void WillDetach(tabs::TabInterface* tab,
                  tabs::TabInterface::DetachReason reason);

  const raw_ref<tabs::TabInterface> tab_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  jni_zero::ScopedJavaGlobalRef<JThinWebViewHost> j_host_;
  base::CallbackListSubscription will_detach_subscription_;
};

}  // namespace thin_webview::android

#endif  // CHROME_BROWSER_ANDROID_THIN_WEBVIEW_TAB_THIN_WEB_VIEW_HOST_H_
