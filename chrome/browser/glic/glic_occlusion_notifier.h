// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_OCCLUSION_NOTIFIER_H_
#define CHROME_BROWSER_GLIC_GLIC_OCCLUSION_NOTIFIER_H_

#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"

namespace glic {

// The GlicOcclusionNotifier notifies the PictureInPictureOcclusionTracker when
// to track the Glic window for occlusion of important security dialogs.
class GlicOcclusionNotifier : public PanelStateObserver {
 public:
  explicit GlicOcclusionNotifier(GlicInstance& instance);
  GlicOcclusionNotifier(const GlicOcclusionNotifier&) = delete;
  GlicOcclusionNotifier& operator=(const GlicOcclusionNotifier&) = delete;
  ~GlicOcclusionNotifier() override;

  // PanelStateObserver:
  void PanelStateChanged(
      const mojom::PanelState& panel_state,
      const GlicInstanceCoordinator::PanelStateContext& context) override;

 private:
  raw_ref<GlicInstance> glic_instance_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_OCCLUSION_NOTIFIER_H_
