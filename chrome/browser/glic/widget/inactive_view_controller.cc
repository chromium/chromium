// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/inactive_view_controller.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
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
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"

namespace {
constexpr float kBlurRadius = 7.0f;
constexpr float kScrimOpacity = 0.8f;
constexpr float kBlurAspectRatioThreshold = 0.1f;
constexpr base::TimeDelta kAnimationDuration = base::Seconds(2);

// A container for inactive glic view that
// 1) gains focus on click.
// 2) clips the layer tree to the visible bounds.
class InactiveGlicViewContainer : public views::View {
  METADATA_HEADER(InactiveGlicViewContainer, views::View)
 public:
  InactiveGlicViewContainer() {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetPaintToLayer();
    layer()->SetMasksToBounds(true);
    SetClipLayerToVisibleBounds(true);

    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetAccessibleRole(ax::mojom::Role::kPane);
    SetAccessibleName(l10n_util::GetStringUTF16(IDS_GLIC_WINDOW_TITLE));
  }

  ~InactiveGlicViewContainer() override = default;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    RequestFocus();
    return true;
  }
};

BEGIN_METADATA(InactiveGlicViewContainer)
END_METADATA

BASE_FEATURE(kGlicBlurredInactiveViewCard, base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

InactiveViewController::InactiveViewController() {
  animation_ = std::make_unique<gfx::SlideAnimation>(this);
  animation_->SetSlideDuration(kAnimationDuration);
  animation_->SetTweenType(gfx::Tween::EASE_IN_OUT);
}
InactiveViewController::~InactiveViewController() = default;

std::unique_ptr<views::View> InactiveViewController::CreateCardView() {
  auto card_container = std::make_unique<views::BoxLayoutView>();
  card_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  card_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  card_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  card_container->SetInsideBorderInsets(gfx::Insets::VH(0, 8));
  card_container->SetBackground(nullptr);

  auto card_view = std::make_unique<views::BoxLayoutView>();
  card_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  card_view->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  card_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  card_view->SetBetweenChildSpacing(18);
  card_view->SetInsideBorderInsets(gfx::Insets::VH(16, 16));
  card_view->SetPaintToLayer();
  card_view->layer()->SetFillsBoundsOpaquely(false);
  card_view->SetBackground(
      views::CreateRoundedRectBackground(kColorSidePanelBackground, 16));
  card_view->SetVisible(false);
  card_view_tracker_.SetView(card_view.get());

  auto icon = std::make_unique<views::ImageView>(ui::ImageModel::FromImageSkia(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GLIC_BUTTON_ALT_ICON)));
  icon->SetImageSize(gfx::Size(20, 20));
  card_view->AddChildView(std::move(icon));

  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_GLIC_INACTIVE_VIEW_CARD_TEXT));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  label->SetMaximumWidth(302);
  label->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  label->SetEnabledColor(ui::kColorSysOnSurface);
  card_view->AddChildView(std::move(label));

  card_container->AddChildView(std::move(card_view));
  return card_container;
}

std::unique_ptr<views::View> InactiveViewController::CreateView() {
  auto container = std::make_unique<InactiveGlicViewContainer>();

  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetPaintToLayer();
  image_view_ = image_view.get();
  image_view_observation_.Observe(image_view_);
  container->AddChildView(std::move(image_view));

  // Add a scrim over the image.
  auto scrim = std::make_unique<views::View>();
  scrim->SetPaintToLayer();
  scrim->layer()->SetFillsBoundsOpaquely(false);
  scrim->layer()->SetOpacity(0.0f);
  scrim_view_tracker_.SetView(scrim.get());
  container->AddChildView(std::move(scrim));

  if (base::FeatureList::IsEnabled(kGlicBlurredInactiveViewCard)) {
    // Add the card view.
    auto card_container = CreateCardView();
    container->AddChildView(std::move(card_container));
  }

  return container;
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
  CheckForImageDistortion();
  animation_->Reset();
  animation_->Show();
  UpdateImageView();
}

base::WeakPtr<InactiveViewController> InactiveViewController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void InactiveViewController::OnViewBoundsChanged(views::View* observed_view) {
  CheckForImageDistortion();
  UpdateImageView();
}

void InactiveViewController::OnViewIsDeleting(views::View* observed_view) {
  image_view_observation_.Reset();
  image_view_ = nullptr;
}

void InactiveViewController::OnViewThemeChanged(views::View* observed_view) {
  UpdateScrimColor();
}

void InactiveViewController::AnimationProgressed(
    const gfx::Animation* animation) {
  if (!animation) {
    return;
  }

  UpdateScrimOpacity(animation_->GetCurrentValue());
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

  views::View* card_view = card_view_tracker_.view();
  if (card_view) {
    card_view->SetVisible(is_image_distorted_);
  }

  if (is_image_distorted_) {
    image_view_->layer()->SetLayerBlur(kBlurRadius);
  }
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

void InactiveViewController::UpdateScrimOpacity(double animation_value) {
  views::View* scrim_view = scrim_view_tracker_.view();
  if (!scrim_view) {
    return;
  }
  const float opacity =
      gfx::Tween::FloatValueBetween(animation_value, 0.0f, kScrimOpacity);
  scrim_view->layer()->SetOpacity(opacity);
}

void InactiveViewController::CheckForImageDistortion() {
  if (!image_view_ || screenshot_.isNull()) {
    is_image_distorted_ = false;
    return;
  }
  if (image_view_->size().IsEmpty()) {
    is_image_distorted_ = false;
    return;
  }

  // Get aspect ratios.
  const float source_aspect_ratio =
      static_cast<float>(screenshot_.width()) / screenshot_.height();
  const float view_aspect_ratio =
      static_cast<float>(image_view_->width()) / image_view_->height();

  // Check if they are significantly different.
  is_image_distorted_ = std::abs(source_aspect_ratio - view_aspect_ratio) >
                        kBlurAspectRatioThreshold;
}
