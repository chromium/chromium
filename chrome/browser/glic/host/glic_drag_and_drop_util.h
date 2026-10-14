// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_DRAG_AND_DROP_UTIL_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_DRAG_AND_DROP_UTIL_H_

#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"

namespace content {
class RenderFrameHost;
class WebContents;
struct DropData;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Returns true if `drop_data` has a Chrome drag ID (`DropData::drag_id`).
bool IsChromeWebDrag(const content::DropData& drop_data);

// Tab feature that records the page context eligibility and source metadata of
// the originating frame when a web drag starts in the tab's WebContents.
class GlicWebDragObserver : public tabs::ContentsObservingTabFeature {
 public:
  explicit GlicWebDragObserver(tabs::TabInterface& tab);
  ~GlicWebDragObserver() override;

  // content::WebContentsObserver:
  void DidStartDrag(content::RenderFrameHost* source_rfh,
                    const content::DropData& drop_data) override;
};

// Validates the drag source against the eligibility recorded by
// `GlicWebDragObserver::DidStartDrag` and triggers the GLIC Invoke API for a
// drop.
void StartDragAndDropInvoke(content::WebContents* target_web_contents,
                            const content::DropData& drop_data);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_DRAG_AND_DROP_UTIL_H_
