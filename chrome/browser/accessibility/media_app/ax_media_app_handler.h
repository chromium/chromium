// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/geometry/insets.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ash {

class AXMediaAppHandler final
    : private ui::AXModeObserver
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ,
      private screen_ai::ScreenAIInstallState::Observer
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
{
 public:
  explicit AXMediaAppHandler(AXMediaApp* media_app);
  AXMediaAppHandler(const AXMediaAppHandler&) = delete;
  AXMediaAppHandler& operator=(const AXMediaAppHandler&) = delete;
  ~AXMediaAppHandler() override;

  bool IsOcrServiceEnabled() const;
  bool IsAccessibilityEnabled() const;
  void DocumentUpdated(const std::vector<gfx::Insets>& page_locations,
                       const std::vector<uint64_t>& dirty_pages);
  void ViewportUpdated(const gfx::Insets& viewport_box, float scaleFactor);

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  // ui::AXModeObserver:
  void OnAXModeAdded(ui::AXMode mode) override;

 private:
  // `AXMediaApp` should outlive this handler.
  raw_ptr<AXMediaApp> media_app_;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  screen_ai::ScreenAIInstallState::State previous_ocr_install_state_ =
      screen_ai::ScreenAIInstallState::State::kNotDownloaded;
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      screen_ai_component_state_observer_{this};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
};

}  // namespace ash

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_H_
