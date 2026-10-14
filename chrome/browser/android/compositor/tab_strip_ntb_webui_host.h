// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_STRIP_NTB_WEBUI_HOST_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_STRIP_NTB_WEBUI_HOST_H_

#include <cstdint>
#include <memory>

#include "cc/slim/layer.h"
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
class TabStripNtbWebUiHost : public content::WebContentsObserver,
                             public content::WebContentsDelegate {
 public:
  explicit TabStripNtbWebUiHost(TabModel* tab_model);
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

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

 private:
  // The service must outlive |web_contents_|, so that it can hold a raw_ptr to
  // it.
  std::unique_ptr<tabs_api::TabStripService> service_;
  std::unique_ptr<content::WebContents> web_contents_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_TAB_STRIP_NTB_WEBUI_HOST_H_
