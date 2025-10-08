// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/inactive_view_controller.h"

#include "chrome/grit/generated_resources.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace {
constexpr float kBlurRadius = 10.0f;

// A simple view that gains focus on click.
class FocusableView : public views::View {
  METADATA_HEADER(FocusableView, views::View)
 public:
  explicit FocusableView(std::unique_ptr<views::View> child) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(std::move(child));
  }
  ~FocusableView() override = default;

  bool OnMousePressed(const ui::MouseEvent& event) override {
    RequestFocus();
    return true;
  }
};

BEGIN_METADATA(FocusableView)
END_METADATA

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

InactiveViewController::InactiveViewController() = default;
InactiveViewController::~InactiveViewController() = default;

std::unique_ptr<views::View> InactiveViewController::CreateView() {
  auto image_view_container = std::make_unique<views::View>();
  image_view_container->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto image_view = std::make_unique<views::ImageView>();
  image_view_ = image_view.get();
  image_view_observation_.Observe(image_view_);
  image_view_container->AddChildView(std::move(image_view));

  auto focusable_view =
      std::make_unique<FocusableView>(std::move(image_view_container));
  focusable_view->SetAccessibleRole(ax::mojom::Role::kPane);
  focusable_view->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_GLIC_WINDOW_TITLE));
  return focusable_view;
}

void InactiveViewController::CaptureScreenshot(
    content::WebContents* glic_webui_contents) {
  if (!glic_webui_contents) {
    OnScreenshotCaptured(gfx::Image());
    return;
  }

  content::RenderWidgetHostView* render_widget_host_view =
      glic_webui_contents->GetRenderWidgetHostView();
  if (!render_widget_host_view) {
    OnScreenshotCaptured(gfx::Image());
    return;
  }

  render_widget_host_view->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindOnce(
          [](base::WeakPtr<InactiveViewController> weak_ptr,
             const viz::CopyOutputBitmapWithMetadata& result) {
            if (weak_ptr) {
              weak_ptr->OnScreenshotCaptured(
                  gfx::Image::CreateFrom1xBitmap(result.bitmap));
            }
          },
          GetWeakPtr()));
}

void InactiveViewController::OnScreenshotCaptured(gfx::Image screenshot) {
  screenshot_ = screenshot.AsImageSkia();
  UpdateImageView();
}

base::WeakPtr<InactiveViewController> InactiveViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void InactiveViewController::OnViewBoundsChanged(views::View* observed_view) {
  UpdateImageView();
}

void InactiveViewController::OnViewIsDeleting(views::View* observed_view) {
  image_view_observation_.Reset();
  image_view_ = nullptr;
}

void InactiveViewController::UpdateImageView() {
  if (!image_view_ || screenshot_.isNull()) {
    return;
  }
  if (image_view_->size().IsEmpty()) {
    return;
  }
  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      screenshot_, skia::ImageOperations::RESIZE_BEST, image_view_->size());
  image_view_->SetImage(
      ui::ImageModel::FromImageSkia(BlurImage(resized_image)));
}
