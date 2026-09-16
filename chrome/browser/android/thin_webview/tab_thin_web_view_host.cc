// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/thin_webview/tab_thin_web_view_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/window_open_disposition.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/android/thin_webview/jni_headers/ThinWebViewHost_jni.h"

namespace thin_webview::android {

TabThinWebViewHost::TabThinWebViewHost(tabs::TabInterface& tab) : tab_(tab) {
  will_detach_subscription_ = tab.RegisterWillDetach(base::BindRepeating(
      &TabThinWebViewHost::WillDetach, base::Unretained(this)));
}

TabThinWebViewHost::~TabThinWebViewHost() {
  // Clears the delegate and drops `web_contents_` before destroying the view,
  // so that the Java teardown cannot re-enter us as a delegate.
  SetWebContents(nullptr);
}

void TabThinWebViewHost::SetWebContents(content::WebContents* web_contents) {
  if (web_contents == web_contents_) {
    return;
  }
  if (web_contents_) {
    web_contents_->SetDelegate(nullptr);
  }
  web_contents_ = web_contents;
  if (!web_contents) {
    // There is nothing left to display, so release the View's resources.
    DestroyView();
    return;
  }
  web_contents_->SetDelegate(this);
  if (j_host_) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    j_host_->setWebContents(env, web_contents);
  }
}

jni_zero::ScopedJavaLocalRef<JView> TabThinWebViewHost::GetView() {
  if (!web_contents_) {
    return nullptr;
  }

  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (!j_host_) {
    content::WebContents* tab_contents = tab_->GetContents();
    ui::WindowAndroid* window =
        tab_contents ? tab_contents->GetTopLevelNativeWindow() : nullptr;
    if (!window) {
      // The tab has no Activity to create the View in. This happens while the
      // tab is in-between Activities during reparenting. GetView() is called
      // again once the tab has been inserted into its new Activity.
      return nullptr;
    }
    j_host_.Reset(
        ThinWebViewHostJni::createNativeOwned(env, web_contents_, window));
  }

  return j_host_->getView(env);
}

void TabThinWebViewHost::DestroyView() {
  if (j_host_) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    j_host_->destroyInternal(env);
    j_host_.Reset();
  }
}

void TabThinWebViewHost::WillDetach(
    tabs::TabInterface* /*tab*/,
    tabs::TabInterface::DetachReason /*reason*/) {
  // The View (and the WindowAndroid it is bound to) is scoped to the Activity
  // the tab is currently in, so it must be destroyed before the tab leaves it.
  // GetView() recreates it for the tab's new Activity when it is next shown.
  DestroyView();
}

content::WebContents* TabThinWebViewHost::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  auto* browser = tab_->GetBrowserWindowInterface();
  if (!browser) {
    return nullptr;
  }
  content::OpenURLParams new_params = params;
  if (new_params.disposition == WindowOpenDisposition::CURRENT_TAB) {
    // `browser` would interpret CURRENT_TAB as "the window's active tab", but
    // the source here is the side panel, not that tab. Never replace the
    // current tab: open a new one instead, as the desktop Customize Chrome
    // side panel does in customize_chrome_page_handler.cc.
    new_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }
  return browser->OpenURL(new_params, std::move(navigation_handle_callback));
}

}  // namespace thin_webview::android
