// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_UNTRUSTED_HANDLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_UNTRUSTED_HANDLER_H_

#include <stdint.h>

#include <vector>

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
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

#include "base/containers/queue.h"
#include "base/sequence_checker.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ui {
struct AXActionData;
}  // namespace ui

namespace ash {

class AXMediaAppUntrustedHandler : private ui::AXActionHandlerBase,
                          public media_app_ui::mojom::OcrUntrustedPageHandler,
                          private ui::AXModeObserver
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ,
                          private screen_ai::ScreenAIInstallState::Observer
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
{
 public:
  AXMediaAppUntrustedHandler(
      content::BrowserContext& context,
      mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page);
  AXMediaAppUntrustedHandler(const AXMediaAppUntrustedHandler&) = delete;
  AXMediaAppUntrustedHandler& operator=(
      const AXMediaAppUntrustedHandler&) = delete;
  ~AXMediaAppUntrustedHandler() override;

  bool IsOcrServiceEnabled() const;
  bool IsAccessibilityEnabled() const;
  void DocumentUpdated(const std::vector<gfx::Insets>& page_locations,
                       const std::vector<uint64_t>& dirty_pages);
  void ViewportUpdated(const gfx::Insets& viewport_box, float scaleFactor);

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  const std::vector<std::unique_ptr<ui::AXTreeManager>>& GetPagesForTesting() {
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

  // TODO(b/309860428): Delete once AXMediaApp is deleted.
  void SetMediaAppForTesting(AXMediaApp* media_app) { media_app_ = media_app; }

 protected:
  // `AXMediaApp` should outlive this handler.
  // TODO(b/309860428): Delete once AXMediaApp is deleted.
  raw_ptr<AXMediaApp> media_app_;

 private:
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void UpdatePageLocation(uint64_t page_index,
                          const gfx::Insets& page_location);
  void OcrNextDirtyPageIfAny();
  void OnPageOcred(uint64_t dirty_page_index,
                   const ui::AXTreeUpdate& tree_update);
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

  std::vector<gfx::Insets> page_locations_;
  // This `BrowserContext` will always outlive the WebUI, so this is safe.
  raw_ref<content::BrowserContext> browser_context_;
  mojo::Remote<media_app_ui::mojom::OcrUntrustedPage> media_app_page_;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  bool is_ocr_service_enabled_for_testing_ = false;
  screen_ai::ScreenAIInstallState::State screen_ai_install_state_ =
      screen_ai::ScreenAIInstallState::State::kNotDownloaded;
  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          screen_ai::ScreenAIInstallState::Observer>
      screen_ai_component_state_observer_{this};
  base::queue<uint64_t> dirty_page_indices_;
  ui::AXTreeManager document_;
  std::vector<std::unique_ptr<ui::AXTreeManager>> pages_;
  mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> screen_ai_annotator_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AXMediaAppUntrustedHandler> weak_ptr_factory_{this};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
};

}  // namespace ash

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_UNTRUSTED_HANDLER_H_
