// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_UNTRUSTED_HANDLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_UNTRUSTED_HANDLER_H_

#include <memory.h>
#include <stdint.h>
#include <vector>

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_action_handler_base.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_mode_observer.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
struct AXActionData;
}  // namespace ui

namespace ash {

struct AXMediaAppPageMetadata : ash::media_app_ui::mojom::PageMetadata {
  // The page number of the page that this metadata describes. 1-indexed (0
  // indexed pages are 'deleted' pages).
  uint32_t page_num;
};

class AXMediaAppUntrustedHandler : private ui::AXActionHandlerBase,
                          public media_app_ui::mojom::OcrUntrustedPageHandler,
                          private ui::AXModeObserver
{
 public:
  AXMediaAppUntrustedHandler(
      content::BrowserContext& context,
      mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page);
  AXMediaAppUntrustedHandler(const AXMediaAppUntrustedHandler&) = delete;
  AXMediaAppUntrustedHandler& operator=(
      const AXMediaAppUntrustedHandler&) = delete;
  ~AXMediaAppUntrustedHandler() override;

  virtual bool IsOcrServiceEnabled() const;
  bool IsAccessibilityEnabled() const;

  void OnOCRServiceInitialized(bool successful);

  // ui::AXActionHandlerBase:
  void PerformAction(const ui::AXActionData& action_data) override;

  // ui::AXModeObserver:
  void OnAXModeAdded(ui::AXMode mode) override;

  // ash::media_app_ui::mojom::OcrUntrustedPageHandler:
  void PageMetadataUpdated(
      const std::vector<ash::media_app_ui::mojom::PageMetadataPtr>
          page_metadata) override;
  void PageContentsUpdated(const std::string& dirty_page_id) override;
  void ViewportUpdated(const gfx::RectF& viewport_box,
                       float scale_factor) override;

 protected:
  void PushDirtyPage(const std::string& dirty_page_id);
  std::string PopDirtyPage();
  virtual void OcrNextDirtyPageIfAny();

  // `AXMediaApp` should outlive this handler.
  // TODO(b/309860428): Delete once AXMediaApp is deleted.
  raw_ptr<AXMediaApp> media_app_;
  ui::AXTreeManager document_;
  std::map<const std::string, AXMediaAppPageMetadata> page_metadata_;
  std::map<const std::string, std::unique_ptr<ui::AXTreeManager>> pages_;
  mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> screen_ai_annotator_;

 private:
  void UpdateDocumentTree();
  void UpdatePageLocation(const std::string& page_id,
                          const gfx::RectF& page_location);
  void OnPageOcred(const std::string& dirty_page_id,
                   const ui::AXTreeUpdate& tree_update);

  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      ax_mode_observation_{this};

  // This `BrowserContext` will always outlive the WebUI, so this is safe.
  raw_ref<content::BrowserContext> browser_context_;
  mojo::Remote<media_app_ui::mojom::OcrUntrustedPage> media_app_page_;

  base::circular_deque<std::string> dirty_page_ids_;
  ui::AXTreeID document_tree_id_ = ui::AXTreeID::CreateNewAXTreeID();
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AXMediaAppUntrustedHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_UNTRUSTED_HANDLER_H_
