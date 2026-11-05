// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_PREWARMER_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_PREWARMER_H_

#include <memory>

#include "base/timer/timer.h"
#include "chrome/browser/glic/selection/selection_overlay.mojom.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class ColorProviderSource;
}  // namespace ui

namespace glic {

// Holds a selection overlay WebContents that started loading before a session,
// so the session can show it without waiting for the WebUI to boot. The
// WebContents is not attached to any view until it is taken. It is dropped as
// soon as its renderer goes away.
class SelectionOverlayPrewarmer : public content::WebContentsObserver {
 public:
  SelectionOverlayPrewarmer();
  SelectionOverlayPrewarmer(const SelectionOverlayPrewarmer&) = delete;
  SelectionOverlayPrewarmer& operator=(const SelectionOverlayPrewarmer&) =
      delete;
  ~SelectionOverlayPrewarmer() override;

  // Keeps `contents`, which is already loading the overlay WebUI, for a
  // session with `interaction_options`. Drops any previous prewarm. Uses the
  // colors of `color_source`, the browser widget it will be shown in, so
  // attaching the WebContents later doesn't restyle it.
  void Start(std::unique_ptr<content::WebContents> contents,
             ui::ColorProviderSource& color_source,
             selection::InteractionOptionsPtr interaction_options);

  // Keeps the current prewarm for another full timeout.
  void Extend();

  void Cancel();

  // Whether the prewarm is for `interaction_options` and hasn't shown an error
  // page.
  bool IsUsable(
      const selection::InteractionOptionsPtr& interaction_options) const;

  // Whether `contents` is the prewarmed WebContents.
  bool Contains(const content::WebContents* contents) const;

  selection::InteractionOptionsPtr GetInteractionOptions() const;

  std::unique_ptr<content::WebContents> Take();

 private:
  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  std::unique_ptr<content::WebContents> contents_;
  selection::InteractionOptionsPtr interaction_options_;
  base::OneShotTimer timeout_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_OVERLAY_PREWARMER_H_
