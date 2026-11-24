// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LENS_GOOGLE_SEARCH_PROMPT_ENDPOINT_H_
#define CHROME_BROWSER_LENS_GOOGLE_SEARCH_PROMPT_ENDPOINT_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/selection/prompt_endpoint.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

namespace lens {

struct ContextualInputData;

// Enables routing smart selection prompts to Google Search in the side panel.
BASE_DECLARE_FEATURE(kSmartSelectionGoogleSearch);

// Routes Smart Selection prompts to the Google Search side panel
// (`ContextualTasks` / Search in Chrome) using the same multimodal search
// pipeline that Lens integrates with.
class GoogleSearchPromptEndpoint : public ::selection::PromptEndpoint {
 public:
  GoogleSearchPromptEndpoint();
  ~GoogleSearchPromptEndpoint() override;

  GoogleSearchPromptEndpoint(const GoogleSearchPromptEndpoint&) = delete;
  GoogleSearchPromptEndpoint& operator=(const GoogleSearchPromptEndpoint&) =
      delete;

  // Converts pixel-space `AreaOfInterest::bounds` into a normalized
  // `CenterRotatedBox` suitable for Lens multimodal cropping. Exposed for unit
  // tests.
  static mojom::CenterRotatedBoxPtr ConvertBoundsToCenterRotatedBox(
      const std::variant<gfx::Rect, std::vector<gfx::Point>>& bounds,
      const gfx::Size& image_size);

  // Builds `ContextualInputData` from the captured screenshot and page context
  // in `page_context` without re-capturing from the renderer. Exposed for unit
  // tests.
  static std::unique_ptr<ContextualInputData> BuildContextualInputData(
      tabs::TabInterface& tab,
      const ::selection::PageContext& page_context);

  // Builds the `CreateSearchUrlRequestInfo` for a standard Google Search SRP
  // request (`SearchUrlType::kStandard`). Exposed for unit tests.
  static std::unique_ptr<contextual_search::ContextualSearchContextController::
                             CreateSearchUrlRequestInfo>
  BuildSearchUrlRequestInfo(const std::string& prompt,
                            const base::UnguessableToken& context_token,
                            const SkBitmap& screenshot,
                            mojom::CenterRotatedBoxPtr region_box);

  // ::selection::PromptEndpoint:
  ::selection::mojom::PromptEndpointId GetId() const override;
  bool IsAvailable(tabs::TabInterface& tab) const override;
  std::u16string GetDisplayName() const override;
  std::u16string GetPlaceholder() const override;
  void Prepare(tabs::TabInterface& tab) override;
  void CancelPendingRequests() override;
  void Submit(tabs::TabInterface& tab,
              std::string prompt,
              ::selection::PageContextWithSelection context) override;

 private:
  static void ClosePendingGhostLoaderIfNeeded(
      base::WeakPtr<tabs::TabInterface> weak_tab,
      std::optional<base::Uuid> pending_task_id);

  void OnSearchUrlCreated(base::WeakPtr<tabs::TabInterface> weak_tab, GURL url);

  // Retains ownership of a newly created session handle when the side panel was
  // already open without a panel handle and `InitSidePanelWithGhostLoader` was
  // skipped, until `StartTaskUiInSidePanel` adopts it.
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      pending_session_handle_;

  // Tracks the tab and task ID whose side panel was opened with a ghost loader
  // via `InitSidePanelWithGhostLoader` while waiting for `OnSearchUrlCreated`.
  // Cleared once `OnSearchUrlCreated` runs or `CancelPendingRequests` cleans up
  // the pending ghost loader.
  base::WeakPtr<tabs::TabInterface> pending_ghost_loader_tab_;
  std::optional<base::Uuid> pending_ghost_loader_task_id_;

  base::WeakPtrFactory<GoogleSearchPromptEndpoint> weak_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_LENS_GOOGLE_SEARCH_PROMPT_ENDPOINT_H_
