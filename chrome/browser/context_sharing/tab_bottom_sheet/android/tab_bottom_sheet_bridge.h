// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list_types.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_client_type.h"

class TabAndroid;

namespace ui {
class WindowAndroid;
}

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
    // Called to notify that the bottom sheet has been closed. This could happen
    // due to various reasons, e.g. explicit request from the client, tab
    // switch, or swapping with another bottom sheet.
    virtual void OnClosed() = 0;

    // Called when the bottom sheet opened, or when the bottom sheet state
    // changes. Here expanded means full/half state, with peek being collapsed
    // state.
    virtual void OnOpened(bool is_expanded) = 0;

    // Called when the bottom sheet has been closed temporarily in java. Expect
    // an onOpened or onClosed in the future if this is called.
    virtual void OnSuppressed() = 0;
  };

  // Creates a bridge to the Java `TabBottomSheetNativeInterface`.
  explicit TabBottomSheetBridge(Observer* observer,
                                tabs::TabInterface* tab,
                                TabBottomSheetClientType client_type);
  ~TabBottomSheetBridge();

  // Sets or updates the WebContents displayed in the bottom sheet.
  void SetWebContents(content::WebContents* web_contents);

  // Triggers the bottom sheet to display on screen.
  // Returns true if the bottom sheet was successfully shown. It returns early
  // if there is no CoBrowseViews, so the caller should make sure that the
  // WebContents are set using SetWebContents() before calling Show().
  bool Show(bool animate, bool starts_expanded);

  // Triggers the bottom sheet to hide and clears the web contents.
  void Close(bool animate);

  // Called by Java when the bottom sheet is closed.
  void OnClosed(JNIEnv* env);

  // Called by Java when the bottom sheet is suppressed.
  void OnSuppressed(JNIEnv* env);

  // Called by Java when the bottom sheet is opened, or when the bottom sheet
  // state changes.
  void OnOpened(JNIEnv* env, bool is_expanded);

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
  TabBottomSheetClientType client_type_;
  raw_ptr<ui::WindowAndroid> window_android_ = nullptr;
};

}  // namespace context_sharing

#endif  // CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_BRIDGE_H_
