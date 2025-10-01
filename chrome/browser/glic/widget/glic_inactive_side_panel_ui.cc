// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"

#include "base/notimplemented.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace glic {

namespace {
constexpr float kBlurRadius = 10.0f;

gfx::ImageSkia BlurImage(gfx::ImageSkia image) {
  SkBitmap blurred_bitmap;
  const SkBitmap* bitmap = image.bitmap();
  if (bitmap) {
    SkImageInfo info = bitmap->info();
    blurred_bitmap.allocPixels(info);
    SkCanvas canvas(blurred_bitmap);
    SkPaint paint;
    paint.setImageFilter(
        SkImageFilters::Blur(kBlurRadius, kBlurRadius, nullptr));
    canvas.drawImage(SkImages::RasterFromBitmap(*bitmap), 0, 0,
                     SkSamplingOptions(), &paint);
  }
  return gfx::ImageSkia::CreateFrom1xBitmap(blurred_bitmap);
}
}  // namespace

// static
std::unique_ptr<GlicInactiveSidePanelUi> GlicInactiveSidePanelUi::From(
    const GlicSidePanelUi& active_ui,
    base::WeakPtr<tabs::TabInterface> tab) {
  // Using `new` to access a private constructor.
  auto inactive_side_panel = base::WrapUnique(new GlicInactiveSidePanelUi(tab));
  inactive_side_panel->VisibilityChanged(/*visible=*/true);

  // Capture screenshot asynchronously and update the inactive panel.
  active_ui.TakeScreenshot(
      base::BindOnce(&GlicInactiveSidePanelUi::OnScreenshotCaptured,
                     inactive_side_panel->weak_ptr_factory_.GetWeakPtr()));

  return inactive_side_panel;
}

GlicInactiveSidePanelUi::GlicInactiveSidePanelUi(
    base::WeakPtr<tabs::TabInterface> tab,
    const GlicSidePanelUi& active_ui)
    : tab_(tab) {
  if (!tab_ || !tab_->GetTabFeatures()) {
    return;
  }

  auto* glic_side_panel_coordinator =
      tab_->GetTabFeatures()->glic_side_panel_coordinator();

  panel_visibility_subscription_ =
      glic_side_panel_coordinator->AddVisibilityCallback(
          base::BindRepeating(&GlicInactiveSidePanelUi::VisibilityChanged,
                              weak_ptr_factory_.GetWeakPtr()));

  glic_side_panel_coordinator->SetContentsView(CreateView(tab_));
}

GlicInactiveSidePanelUi::GlicInactiveSidePanelUi(
    base::WeakPtr<tabs::TabInterface> tab)
    : tab_(tab) {
  if (!tab_ || !tab_->GetTabFeatures()) {
    return;
  }

  auto* glic_side_panel_coordinator =
      tab_->GetTabFeatures()->glic_side_panel_coordinator();

  panel_visibility_subscription_ =
      glic_side_panel_coordinator->AddVisibilityCallback(
          base::BindRepeating(&GlicInactiveSidePanelUi::VisibilityChanged,
                              weak_ptr_factory_.GetWeakPtr()));

  glic_side_panel_coordinator->SetContentsView(CreateView(tab_));
}

std::unique_ptr<views::View> GlicInactiveSidePanelUi::CreateView(
    base::WeakPtr<tabs::TabInterface> tab) {
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view_tracker_.SetView(image_view.get());  // Track the image view
  return image_view;
}

GlicInactiveSidePanelUi::~GlicInactiveSidePanelUi() = default;

Host::EmbedderDelegate* GlicInactiveSidePanelUi::GetHostEmbedderDelegate() {
  // This should not be called for an inactive embedder. The delegate is managed
  // by the GlicInstanceImpl.
  NOTREACHED();
}

bool GlicInactiveSidePanelUi::IsShowing() const {
  return is_showing_;
}

void GlicInactiveSidePanelUi::Show() {
  if (!tab_ || !tab_->GetTabFeatures()) {
    return;
  }
  SidePanelRegistry* registry = tab_->GetTabFeatures()->side_panel_registry();
  SidePanelEntry* glic_entry =
      registry->GetEntryForKey(SidePanelEntry::Key(SidePanelEntry::Id::kGlic));
  if (glic_entry) {
    registry->SetActiveEntry(glic_entry);
  }
}

void GlicInactiveSidePanelUi::Close() {
  // TODO: implement close.
  NOTIMPLEMENTED();
}

std::unique_ptr<GlicUiEmbedder>
GlicInactiveSidePanelUi::CreateInactiveEmbedder() const {
  NOTREACHED() << "The embedder is already inactive.";
}

void GlicInactiveSidePanelUi::VisibilityChanged(bool visible) {
  is_showing_ = visible;
}

void GlicInactiveSidePanelUi::OnScreenshotCaptured(gfx::Image screenshot) {
  screenshot_ = screenshot.AsImageSkia();
  UpdateImageView();
}

void GlicInactiveSidePanelUi::UpdateImageView() {
  if (!image_view_tracker_.view() || screenshot_.isNull()) {
    return;
  }
  // Post task to a background thread to blur the image.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&BlurImage, screenshot_),
      base::BindOnce(&GlicInactiveSidePanelUi::OnImageBlurred,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlicInactiveSidePanelUi::OnImageBlurred(gfx::ImageSkia blurred_image) {
  if (!image_view_tracker_.view()) {
    return;
  }
  auto* image_view = static_cast<views::ImageView*>(image_view_tracker_.view());
  image_view->SetImage(ui::ImageModel::FromImageSkia(blurred_image));
  image_view->SetImageSize(screenshot_.size());
}

}  // namespace glic
