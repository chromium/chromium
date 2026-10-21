// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_suggestion_tool.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/browser/selection/suggestion.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace lens {

BASE_FEATURE(kLensSelectionSuggestions, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

gfx::Rect GetBoundsRect(
    const std::variant<gfx::Rect, std::vector<gfx::Point>>& bounds) {
  if (const gfx::Rect* const rect = std::get_if<gfx::Rect>(&bounds)) {
    return *rect;
  }
  const auto& points = std::get<std::vector<gfx::Point>>(bounds);
  if (points.empty()) {
    return gfx::Rect();
  }
  int min_x = points.front().x();
  int min_y = points.front().y();
  int max_x = min_x;
  int max_y = min_y;
  for (const gfx::Point& point : points) {
    min_x = std::min(min_x, point.x());
    min_y = std::min(min_y, point.y());
    max_x = std::max(max_x, point.x());
    max_y = std::max(max_y, point.y());
  }
  return gfx::Rect(min_x, min_y, max_x - min_x, max_y - min_y);
}

class LensSuggestion : public ::selection::Suggestion {
 public:
  LensSuggestion(tabs::TabInterface& tab,
                 std::u16string label,
                 gfx::Rect bounds,
                 SkBitmap screenshot)
      : tab_(tab),
        label_(std::move(label)),
        bounds_(bounds),
        screenshot_(std::move(screenshot)) {}
  ~LensSuggestion() override = default;

  // ::selection::Suggestion:
  const std::u16string& GetLabel() const override { return label_; }

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS;
  }

  void OnSuggestionPresented() override {}

  void OnSuggestionExecuted() override {
    LensSearchController* const controller =
        LensSearchController::From(&tab_.get());
    if (!controller) {
      return;
    }

    gfx::Rect viewport_bounds(screenshot_.width(), screenshot_.height());
    if (viewport_bounds.IsEmpty()) {
      if (content::WebContents* const web_contents = tab_->GetContents()) {
        viewport_bounds = gfx::Rect(web_contents->GetViewBounds().size());
      }
    }
    if (viewport_bounds.IsEmpty() || bounds_.IsEmpty()) {
      return;
    }

    SkBitmap region_bitmap;
    if (!screenshot_.drawsNothing()) {
      region_bitmap = lens::CropBitmapToRegion(
          screenshot_, lens::GetCenterRotatedBoxFromTabViewAndImageBounds(
                           viewport_bounds, viewport_bounds, bounds_));
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LensSearchController::OpenLensOverlayWithPendingRegionFromBounds,
            controller->GetWeakPtr(),
            lens::LensOverlayInvocationSource::kSelectionSuggestion,
            viewport_bounds, viewport_bounds, bounds_,
            std::move(region_bitmap)));
  }

  ::selection::mojom::ActionPtr GetAction() const override {
    return ::selection::mojom::Action::NewHandoff(
        ::selection::mojom::Handoff::New());
  }

 private:
  const raw_ref<tabs::TabInterface> tab_;
  const std::u16string label_;
  const gfx::Rect bounds_;
  const SkBitmap screenshot_;
};

}  // namespace

LensSuggestionTool::LensSuggestionTool(tabs::TabInterface& tab) : tab_(tab) {
  if (::selection::SuggestionService* const suggestion_service =
          ::selection::SuggestionService::From(&tab_.get())) {
    suggestion_service->RegisterTool(this);
  }
}

LensSuggestionTool::~LensSuggestionTool() {
  if (::selection::SuggestionService* const suggestion_service =
          ::selection::SuggestionService::From(&tab_.get())) {
    suggestion_service->UnregisterTool(this);
  }
}

LensSuggestionTool::ToolId LensSuggestionTool::GetToolId() const {
  return optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS;
}

std::unique_ptr<::selection::Suggestion> LensSuggestionTool::CreateSuggestion(
    const ::selection::AreaOfInterest& processed_area,
    const optimization_guide::proto::SmartSelectionSuggestion&
        server_suggestion) {
  const gfx::Rect bounds = GetBoundsRect(processed_area.bounds);
  if (bounds.IsEmpty()) {
    return nullptr;
  }
  std::u16string label = server_suggestion.label().empty()
                             ? l10n_util::GetStringUTF16(
                                   IDS_LENS_SELECTION_SUGGESTION_DEFAULT_LABEL)
                             : base::UTF8ToUTF16(server_suggestion.label());
  return std::make_unique<LensSuggestion>(*tab_, std::move(label), bounds,
                                          processed_area.screenshot);
}

}  // namespace lens
