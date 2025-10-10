// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/inactive_view_controller.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace {
constexpr float kBlurRadius = 7.0f;
constexpr float kScrimOpacity = 0.8f;
constexpr float kBlurAspectRatioThreshold = 0.05f;

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

}  // namespace

InactiveViewController::InactiveViewController() = default;
InactiveViewController::~InactiveViewController() = default;

std::unique_ptr<views::View> InactiveViewController::CreateView() {
  auto image_view_container = std::make_unique<views::View>();
  image_view_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  image_view_container->SetPaintToLayer();
  image_view_container->layer()->SetMasksToBounds(true);

  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetPaintToLayer();
  image_view_ = image_view.get();
  image_view_observation_.Observe(image_view_);
  image_view_container->AddChildView(std::move(image_view));

  // Add a scrim over the image.
  auto scrim = std::make_unique<views::View>();
  scrim->SetPaintToLayer();
  scrim->layer()->SetFillsBoundsOpaquely(false);
  scrim->layer()->SetOpacity(kScrimOpacity);
  scrim_view_tracker_.SetView(scrim.get());
  image_view_container->AddChildView(std::move(scrim));

  auto focusable_view =
      std::make_unique<FocusableView>(std::move(image_view_container));
  focusable_view->SetBackground(nullptr);
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

void InactiveViewController::OnViewThemeChanged(views::View* observed_view) {
  UpdateScrimColor();
}

void InactiveViewController::UpdateImageView() {
  if (!image_view_ || screenshot_.isNull()) {
    return;
  }
  if (image_view_->size().IsEmpty()) {
    return;
  }

  // Get aspect ratios.
  const float source_aspect_ratio =
      static_cast<float>(screenshot_.width()) / screenshot_.height();
  const float view_aspect_ratio =
      static_cast<float>(image_view_->width()) / image_view_->height();

  // Check if they are significantly different.
  const bool should_blur = std::abs(source_aspect_ratio - view_aspect_ratio) >
                           kBlurAspectRatioThreshold;
  image_view_->layer()->SetLayerBlur(should_blur ? kBlurRadius : 0.0f);

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      screenshot_, skia::ImageOperations::RESIZE_BEST, image_view_->size());

  image_view_->SetImage(ui::ImageModel::FromImageSkia(resized_image));
}

void InactiveViewController::UpdateScrimColor() {
  views::View* scrim_view = scrim_view_tracker_.view();
  if (!scrim_view) {
    return;
  }

  const ui::ColorProvider* color_provider = scrim_view->GetColorProvider();
  const SkColor background_color =
      color_provider->GetColor(kColorSidePanelBackground);

  color_utils::HSL hsl;
  color_utils::SkColorToHSL(background_color, &hsl);
  hsl.s = 0;

  scrim_view->SetBackground(
      views::CreateSolidBackground(color_utils::HSLToSkColor(hsl, 255)));
}
