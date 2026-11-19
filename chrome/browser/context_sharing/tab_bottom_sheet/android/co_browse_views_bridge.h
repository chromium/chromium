// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_CO_BROWSE_VIEWS_BRIDGE_H_
#define CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_CO_BROWSE_VIEWS_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_container_type.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_client_type.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/color/color_id.h"

class TabAndroid;

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

namespace context_sharing {

// Bridge for managing CoBrowseViews on Android from C++.
// Empty-placeholder visibility is gated on `tab_` being active, which assumes
// these views belong to a single tab. Window-scoped hosts (such as Contextual
// Tasks) reuse one bridge across tabs and do not use the empty placeholder.
class CoBrowseViewsBridge : public content::WebContentsObserver {
 public:
  // Returns the Java View object from a CoBrowseViews object.
  static base::android::ScopedJavaLocalRef<jobject> GetViewFromCoBrowseViews(
      const base::android::JavaRef<jobject>& java_co_browse_views);

  CoBrowseViewsBridge(
      tabs::TabInterface& tab,
      context_sharing::TabBottomSheetClientType client_type,
      context_sharing::CoBrowseContainerType container_type,
      const base::android::JavaRef<jobject>& bottom_sheet_content_provider,
      bool enable_pinch_to_zoom,
      ui::ColorId background_color_id,
      ui::ColorId placeholder_elem_color_id,
      base::RepeatingClosure close_callback = {});
  ~CoBrowseViewsBridge() override;

  CoBrowseViewsBridge(const CoBrowseViewsBridge&) = delete;
  CoBrowseViewsBridge& operator=(const CoBrowseViewsBridge&) = delete;

  // Creates the CoBrowseViews instance using the factory.
  // Returns true if successful.
  bool CreateCoBrowseViews(content::WebContents* web_contents,
                           bool request_focus);

  // Sets the web contents for the view.
  void SetWebContents(content::WebContents* web_contents, bool request_focus);

  // Updates whether the native loading placeholder is shown. While loading, the
  // placeholder covers the web UI even if a `WebContents` is attached.
  void SetIsLoading(bool is_loading);

  // Returns the Java CoBrowseViews object.
  base::android::ScopedJavaLocalRef<jobject> GetCoBrowseViews();

  // Returns true if the Java CoBrowseViews currently holds a non-null
  // WebContents reference.
  bool HasJavaWebContentsForTesting() const;

  // Returns true if the Java CoBrowseViews currently shows the empty
  // placeholder.
  bool IsEmptyPlaceholderVisibleForTesting() const;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  void DestroyCoBrowseViews();
  TabAndroid* GetTabAndroid() const;
  void SetTabActive(bool is_tab_active);
  void OnTabInserted(tabs::TabInterface* tab);
  void OnTabDidActivate(tabs::TabInterface* tab);
  void OnTabWillDeactivate(tabs::TabInterface* tab);
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  const raw_ref<tabs::TabInterface> tab_;
  const context_sharing::TabBottomSheetClientType client_type_;
  const context_sharing::CoBrowseContainerType container_type_;
  const bool enable_pinch_to_zoom_;
  const ui::ColorId background_color_id_;
  const ui::ColorId placeholder_elem_color_id_;
  base::RepeatingClosure close_callback_;
  bool is_loading_ = false;
  bool is_tab_active_ = false;
  base::android::ScopedJavaGlobalRef<jobject> java_co_browse_views_;
  base::android::ScopedJavaGlobalRef<jobject> bottom_sheet_content_provider_;
  raw_ptr<ui::WindowAndroid> window_android_ = nullptr;
  base::CallbackListSubscription tab_insert_subscription_;
  base::CallbackListSubscription tab_did_activate_subscription_;
  base::CallbackListSubscription tab_will_deactivate_subscription_;
  base::CallbackListSubscription tab_will_detach_subscription_;
};

}  // namespace context_sharing

#endif  // CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_CO_BROWSE_VIEWS_BRIDGE_H_
