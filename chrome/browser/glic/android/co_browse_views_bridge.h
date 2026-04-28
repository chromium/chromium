// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ANDROID_CO_BROWSE_VIEWS_BRIDGE_H_
#define CHROME_BROWSER_GLIC_ANDROID_CO_BROWSE_VIEWS_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_client_type.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace glic {

// Bridge for managing CoBrowseViews on Android from C++.
class CoBrowseViewsBridge {
 public:
  explicit CoBrowseViewsBridge(
      tabs::TabInterface& tab,
      context_sharing::TabBottomSheetClientType client_type);
  ~CoBrowseViewsBridge();

  CoBrowseViewsBridge(const CoBrowseViewsBridge&) = delete;
  CoBrowseViewsBridge& operator=(const CoBrowseViewsBridge&) = delete;

  // Creates the CoBrowseViews instance using the factory.
  // Returns true if successful.
  bool CreateCoBrowseViews(content::WebContents* web_contents);

  // Sets the web contents for the view.
  void SetWebContents(content::WebContents* web_contents);

  // Returns the Java View object.
  base::android::ScopedJavaLocalRef<jobject> GetView();

 private:
  const raw_ref<tabs::TabInterface> tab_;
  const context_sharing::TabBottomSheetClientType client_type_;
  base::android::ScopedJavaGlobalRef<jobject> java_co_browse_views_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_CO_BROWSE_VIEWS_BRIDGE_H_
