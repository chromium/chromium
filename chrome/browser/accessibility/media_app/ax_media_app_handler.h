// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/ax_action_handler_base.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include <memory.h>
#include <map>

#include "base/containers/queue.h"
#include "base/sequence_checker.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ui {

struct AXActionData;

}  // namespace ui

namespace ash {

class AXMediaAppHandler final
    : private ui::AXActionHandlerBase,
      private ui::AXModeObserver
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
  const std::map<uint64_t, std::unique_ptr<ui::AXTreeManager>>&
  GetPagesForTesting() {
    return pages_;
  }

  void SetIsOcrServiceEnabledForTesting() {
    is_ocr_service_enabled_for_testing_ = true;
  }

  void SetScreenAIAnnotatorForTesting(
      mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
          screen_ai_annotator);

  void FlushForTesting();

  // ScreenAIInstallState::Observer:
  void StateChanged(screen_ai::ScreenAIInstallState::State state) override;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  // ui::AXActionHandlerBase:
  void PerformAction(const ui::AXActionData& action_data) override;

  // ui::AXModeObserver:
  void OnAXModeAdded(ui::AXMode mode) override;

 private:
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  struct DirtyPageInfo final {
    DirtyPageInfo(const gfx::Insets& page_location, uint64_t dirty_page_index)
        : page_location(page_location), dirty_page_index(dirty_page_index) {}

    const gfx::Insets page_location;
    uint64_t dirty_page_index;
  };

  void OcrNextDirtyPageIfAny();
  void OnPageOcred(DirtyPageInfo dirty_page_info,
                   const ui::AXTreeUpdate& tree_update);
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  // `AXMediaApp` should outlive this handler.
  raw_ptr<AXMediaApp> media_app_;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  bool is_ocr_service_enabled_for_testing_ = false;
  screen_ai::ScreenAIInstallState::State screen_ai_install_state_ =
      screen_ai::ScreenAIInstallState::State::kNotDownloaded;
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      screen_ai_component_state_observer_{this};
  base::queue<DirtyPageInfo> dirty_pages_;
  ui::AXTreeManager document_;
  std::map<uint64_t, std::unique_ptr<ui::AXTreeManager>> pages_;
  mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> screen_ai_annotator_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AXMediaAppHandler> weak_ptr_factory_{this};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
};

}  // namespace ash

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_H_
