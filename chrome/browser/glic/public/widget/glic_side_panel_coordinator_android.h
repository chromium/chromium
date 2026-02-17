// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_ANDROID_H_
#define CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

class TabAndroid;

namespace glic {

class GlicSidePanelCoordinatorAndroid : public GlicSidePanelCoordinator {
 public:
  explicit GlicSidePanelCoordinatorAndroid(tabs::TabInterface* tab);
  ~GlicSidePanelCoordinatorAndroid() override;

  // GlicSidePanelCoordinator:
  using GlicSidePanelCoordinator::Show;
  void Show(bool suppress_animations) override;
  void SetWebContents(content::WebContents* web_contents) override;
  void Close(const CloseOptions& options) override;
  bool IsShowing() const override;
  State state() override;
  base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State state)> callback) override;
  int GetPreferredWidth() override;
  bool IsGlicSidePanelActive() override;

  void OnClose(JNIEnv* env);

 private:
  void SetState(State state);
  void OnTabDidActivate(tabs::TabInterface* tab);
  void OnTabWillDeactivate(tabs::TabInterface* tab);
  TabAndroid* GetTabAndroid() const;

  State state_ = State::kClosed;
  base::RepeatingCallbackList<void(State)> state_callbacks_;
  const raw_ref<tabs::TabInterface> tab_;
  base::CallbackListSubscription did_activate_subscription_;
  base::CallbackListSubscription will_deactivate_subscription_;
  base::android::ScopedJavaGlobalRef<jobject> java_interface_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_ANDROID_H_
