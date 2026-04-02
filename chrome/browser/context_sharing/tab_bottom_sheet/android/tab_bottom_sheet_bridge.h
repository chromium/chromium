// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list_types.h"

class TabAndroid;

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace context_sharing {

// Connects a C++ implementation to the Java TabBottomSheetNativeInterface.
// This class abstracts away the JNI boundary to allow the underlying native
// feature controller to show and close a bottom sheet without exposing JNI
// dependencies. It also manages the state of the CoBrowseViews object.
class TabBottomSheetBridge {
 public:
  // Observer for bottom sheet lifecycle events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnClose() = 0;
  };

  // Creates a bridge to the Java `TabBottomSheetNativeInterface`.
  explicit TabBottomSheetBridge(Observer* observer, tabs::TabInterface* tab);
  ~TabBottomSheetBridge();

  // Sets or updates the WebContents displayed in the bottom sheet.
  void SetWebContents(content::WebContents* web_contents);

  // Triggers the bottom sheet to display on screen.
  // Returns true if the bottom sheet was successfully shown. It returns early
  // if there is no CoBrowseViews, so the caller should make sure that the
  // WebContents are set using SetWebContents() before calling Show().
  bool Show(bool animate, bool starts_expanded);

  // Triggers the bottom sheet to hide and clears the web contents.
  void Close();

  // Called by Java when the bottom sheet is closed.
  void OnClose(JNIEnv* env);

 private:
  // Resets and creates the CoBrowseViews object with the attached WebContents.
  void CreateCoBrowseViews(content::WebContents* web_contents);

  // Unattaches the WebContents and destroys the CoBrowseViews.
  void DestroyCoBrowseViews();

  TabAndroid* GetTabAndroid() const;

  raw_ptr<Observer> observer_;
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
  base::android::ScopedJavaGlobalRef<jobject> co_browse_views_;
  const raw_ref<tabs::TabInterface> tab_;
};

}  // namespace context_sharing

#endif  // CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_BRIDGE_H_
