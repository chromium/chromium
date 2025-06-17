// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_OCCLUSION_NOTIFIER_H_
#define CHROME_BROWSER_GLIC_GLIC_OCCLUSION_NOTIFIER_H_

#include "chrome/browser/glic/widget/glic_window_controller.h"

namespace glic {

// The GlicOcclusionNotifier notifies the PictureInPictureOcclusionTracker when
// to track the Glic window for occlusion of important security dialogs.
class GlicOcclusionNotifier : public GlicWindowController::StateObserver {
 public:
  explicit GlicOcclusionNotifier(GlicWindowController& window_controller);
  GlicOcclusionNotifier(const GlicOcclusionNotifier&) = delete;
  GlicOcclusionNotifier& operator=(const GlicOcclusionNotifier&) = delete;
  ~GlicOcclusionNotifier() override;

  // GlicWindowController::StateObserver:
  void PanelStateChanged(const mojom::PanelState& panel_state,
                         Browser*) override;

 private:
  raw_ref<GlicWindowController> window_controller_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_OCCLUSION_NOTIFIER_H_
