// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_MODAL_ANDROID_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_MODAL_ANDROID_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/glic/browser_ui/glic_modal.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

class Profile;

namespace content {
class NavigationHandle;
class RenderFrameHost;
class RenderWidgetHostView;
class WebContents;
struct OpenURLParams;
}  // namespace content

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace glic {

class GlicModalAndroid : public GlicModal,
                         public content::WebContentsDelegate,
                         public content::WebContentsObserver {
 public:
  GlicModalAndroid();
  ~GlicModalAndroid() override;

  bool ShowInternal(tabs::TabInterface& tab,
                    Profile* profile,
                    ui::WindowAndroid* window_android,
                    Params params);

  // GlicModal:
  void Close() override;
  void Activate() override;
  void ShowContent() override;
  content::WebContents* GetWebContents() const override;
  void SimulateDismissingForTesting() override;

  // content::WebContentsDelegate:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  void CloseContents(content::WebContents* source) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Called from Java:
  void OnDismissed();
  void OnAvailableSizeChanged(JNIEnv* env, int width_dip, int height_dip);

 private:
  void UpdateAutoResize(content::RenderWidgetHostView* rwhv);
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void OnTabWillBecomeHidden(tabs::TabInterface* tab);

  tabs::TabHandle tab_handle_;
  gfx::Size min_size_dip_;
  gfx::Size max_size_dip_;
  gfx::Size available_size_dip_;
  bool size_to_content_ = false;
  base::RepeatingCallback<void(gfx::Size)> on_available_size_changed_;

  std::unique_ptr<content::WebContents> modal_web_contents_;
  base::android::ScopedJavaGlobalRef<jobject> java_coordinator_;
  base::OnceClosure on_closed_;
  base::CallbackListSubscription tab_will_detach_subscription_;
  base::CallbackListSubscription tab_will_become_hidden_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_MODAL_ANDROID_H_
