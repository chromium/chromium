// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_CO_BROWSE_VIEWS_BRIDGE_H_
#define CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_CO_BROWSE_VIEWS_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_container_type.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_client_type.h"

class TabAndroid;

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace ui {
class WindowAndroid;
}

namespace context_sharing {

// Bridge for managing CoBrowseViews on Android from C++.
class CoBrowseViewsBridge {
 public:
  // Returns the Java View object from a CoBrowseViews object.
  static base::android::ScopedJavaLocalRef<jobject> GetViewFromCoBrowseViews(
      const base::android::JavaRef<jobject>& java_co_browse_views);

  explicit CoBrowseViewsBridge(
      tabs::TabInterface& tab,
      context_sharing::TabBottomSheetClientType client_type,
      context_sharing::CoBrowseContainerType container_type,
      const base::android::JavaRef<jobject>& bottom_sheet_content_provider =
          nullptr);
  ~CoBrowseViewsBridge();

  CoBrowseViewsBridge(const CoBrowseViewsBridge&) = delete;
  CoBrowseViewsBridge& operator=(const CoBrowseViewsBridge&) = delete;

  // Creates the CoBrowseViews instance using the factory.
  // Returns true if successful.
  bool CreateCoBrowseViews(content::WebContents* web_contents);

  // Sets the web contents for the view.
  void SetWebContents(content::WebContents* web_contents, bool request_focus);

  // Returns the Java CoBrowseViews object.
  base::android::ScopedJavaLocalRef<jobject> GetCoBrowseViews();

 private:
  void DestroyCoBrowseViews();
  TabAndroid* GetTabAndroid() const;

  const raw_ref<tabs::TabInterface> tab_;
  const context_sharing::TabBottomSheetClientType client_type_;
  const context_sharing::CoBrowseContainerType container_type_;
  base::android::ScopedJavaGlobalRef<jobject> java_co_browse_views_;
  base::android::ScopedJavaGlobalRef<jobject> bottom_sheet_content_provider_;
  raw_ptr<ui::WindowAndroid> window_android_ = nullptr;
};

}  // namespace context_sharing

#endif  // CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_CO_BROWSE_VIEWS_BRIDGE_H_
