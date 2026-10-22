// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_STRIP_NTB_WEBUI_HOST_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_STRIP_NTB_WEBUI_HOST_H_

#include <cstdint>
#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "cc/slim/layer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class TabModel;

namespace content {
class WebContents;
}  // namespace content

namespace tabs_api {
class TabStripService;
}  // namespace tabs_api

namespace android {

// Native counterpart of the Java NtbWebUiCoordinator. Owns the
// TabStripService and the WebContents that renders
// chrome://tabstrip-ntb.top-chrome/, and exposes its compositor layer so that
// TabStripSceneLayer can place it inside the tab strip scene.
class TabStripNtbWebUiHost
    : public content::WebContentsObserver,
      public content::WebContentsDelegate,
      public content::RenderWidgetHost::InputEventObserver,
      public TabModelListObserver {
 public:
  TabStripNtbWebUiHost(TabModel* tab_model,
                       base::TimeTicks activity_creation_time,
                       const base::android::JavaRef<jobject>& java_coordinator);
  TabStripNtbWebUiHost(const TabStripNtbWebUiHost&) = delete;
  TabStripNtbWebUiHost& operator=(const TabStripNtbWebUiHost&) = delete;
  ~TabStripNtbWebUiHost() override;

  void Resize(int32_t width_px, int32_t height_px);
  // Returns the hosted WebContents. It stays owned by this object.
  content::WebContents* GetWebContents();

  // Returns the layer of the hosted WebContents, or nullptr.
  scoped_refptr<cc::slim::Layer> GetLayer();

  // content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void OnFirstContentfulPaintInPrimaryMainFrame(
      base::TimeTicks presentation_time) override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const content::RenderWidgetHost& widget,
                    const blink::WebInputEvent& event,
                    input::InputEventSource source) override;

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* tab_model) override {}
  void OnTabModelRemoved(TabModel* tab_model) override;

 private:
  void Reset();

  // The TabModel the service operates on. Caller is responsible for calling
  // Reset when the TabModel is destroyed.
  raw_ptr<TabModel> tab_model_;
  base::TimeTicks activity_creation_time_;
  bool recorded_first_paint_ = false;
  base::android::ScopedJavaGlobalRef<jobject> java_coordinator_;
  // Declared before |web_contents_| so that it is destroyed after it. Null
  // once |tab_model_| is destroyed.
  std::unique_ptr<tabs_api::TabStripService> service_;
  // Null once |tab_model_| is destroyed, which can happen before this host is
  // destroyed in the case of incognito windows.
  std::unique_ptr<content::WebContents> web_contents_;
  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHost::InputEventObserver>
      input_event_observation_{this};
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_STRIP_NTB_WEBUI_HOST_H_
