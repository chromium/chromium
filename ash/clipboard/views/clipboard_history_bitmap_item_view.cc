// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_delete_button.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The duration of the fade out animation for transitioning the placeholder
// image to rendered HTML.
constexpr base::TimeDelta kFadeOutDurationMs =
    base::TimeDelta::FromMilliseconds(60);

// The duration of the fade in animation for transitioning the placeholder image
// to rendered HTML.
constexpr base::TimeDelta kFadeInDurationMs =
    base::TimeDelta::FromMilliseconds(200);

////////////////////////////////////////////////////////////////////////////////
// FadeImageView
// An ImageView which reacts to updates from ClipboardHistoryResourceManager by
// fading out the old image, and fading in the new image. Used when HTML is done
// rendering. Only expected to transition once in its lifetime.
class FadeImageView : public RoundedImageView,
                      public ui::ImplicitAnimationObserver,
                      public ClipboardHistoryResourceManager::Observer {
 public:
  FadeImageView(const ClipboardHistoryItem* clipboard_history_item,
                const ClipboardHistoryResourceManager* resource_manager,
                base::RepeatingClosure update_callback)
      : RoundedImageView(ClipboardHistoryViews::kImageRoundedCornerRadius,
                         RoundedImageView::Alignment::kCenter),
        resource_manager_(resource_manager),
        clipboard_history_item_(*clipboard_history_item),
        update_callback_(update_callback) {
    resource_manager_->AddObserver(this);
    SetImageFromModel();
    DCHECK(update_callback_);
  }

  FadeImageView(const FadeImageView& rhs) = delete;

  FadeImageView& operator=(const FadeImageView& rhs) = delete;

  ~FadeImageView() override {
    StopObservingImplicitAnimations();
    resource_manager_->RemoveObserver(this);
  }

  // ClipboardHistoryResourceManager::Observer:
  void OnCachedImageModelUpdated(
      const std::vector<base::UnguessableToken>& item_ids) override {
    if (!base::Contains(item_ids, clipboard_history_item_.id()))
      return;

    // Fade the old image out, then swap in the new image.
    DCHECK_EQ(FadeAnimationState::kNoFadeAnimation, animation_state_);
    SetPaintToLayer();
    animation_state_ = FadeAnimationState::kFadeOut;

    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(kFadeOutDurationMs);
    settings.AddObserver(this);
    layer()->SetOpacity(0.0f);
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    switch (animation_state_) {
      case FadeAnimationState::kNoFadeAnimation:
        NOTREACHED();
        return;
      case FadeAnimationState::kFadeOut:
        DCHECK_EQ(0.0f, layer()->opacity());
        animation_state_ = FadeAnimationState::kFadeIn;
        SetImageFromModel();
        {
          ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
          settings.AddObserver(this);
          settings.SetTransitionDuration(kFadeInDurationMs);
          layer()->SetOpacity(1.0f);
        }
        return;
      case FadeAnimationState::kFadeIn:
        DestroyLayer();
        animation_state_ = FadeAnimationState::kNoFadeAnimation;
        return;
    }
  }

  void SetImageFromModel() {
    const gfx::ImageSkia& image =
        *(resource_manager_->GetImageModel(clipboard_history_item_)
              .GetImage()
              .ToImageSkia());
      SetImage(image);

    // When fading in a new image, the ImageView's image has likely changed
    // sizes.
    if (animation_state_ == FadeAnimationState::kFadeIn)
      update_callback_.Run();
  }

  // The different animation states possible when transitioning from one
  // gfx::ImageSkia to the next.
  enum class FadeAnimationState {
    kNoFadeAnimation,
    kFadeOut,
    kFadeIn,
  };

  // The current animation state.
  FadeAnimationState animation_state_ = FadeAnimationState::kNoFadeAnimation;

  // The resource manager, owned by ClipboardHistoryController.
  const ClipboardHistoryResourceManager* const resource_manager_;

  // The ClipboardHistoryItem represented by this class.
  const ClipboardHistoryItem clipboard_history_item_;

  // Used to notify of image changes.
  base::RepeatingClosure update_callback_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryBitmapItemView::BitmapContentsView

class ClipboardHistoryBitmapItemView::BitmapContentsView
    : public ClipboardHistoryBitmapItemView::ContentsView {
 public:
  explicit BitmapContentsView(ClipboardHistoryBitmapItemView* container)
      : ContentsView(container), container_(container) {
    SetLayoutManager(std::make_unique<views::FillLayout>());

    auto image_view = BuildImageView();
    image_view->SetPreferredSize(
        gfx::Size(INT_MAX, ClipboardHistoryViews::kImageViewPreferredHeight));
    image_view_ = AddChildView(std::move(image_view));

    // `border_container_view_` should be above `image_view_`.
    border_container_view_ = AddChildView(std::make_unique<views::View>());

    border_container_view_->SetBorder(views::CreateRoundedRectBorder(
        ClipboardHistoryViews::kImageBorderThickness,
        ClipboardHistoryViews::kImageRoundedCornerRadius,
        gfx::kPlaceholderColor));

    InstallDeleteButton();
  }
  BitmapContentsView(const BitmapContentsView& rhs) = delete;
  BitmapContentsView& operator=(const BitmapContentsView& rhs) = delete;
  ~BitmapContentsView() override = default;

 private:
  // ContentsView:
  ClipboardHistoryDeleteButton* CreateDeleteButton() override {
    auto delete_button_container = std::make_unique<views::View>();
    auto* layout_manager = delete_button_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout_manager->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kEnd);
    layout_manager->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    auto delete_button =
        std::make_unique<ClipboardHistoryDeleteButton>(container_);
    delete_button->SetProperty(
        views::kMarginsKey,
        ClipboardHistoryViews::kBitmapItemDeleteButtonMargins);
    ClipboardHistoryDeleteButton* delete_button_ptr =
        delete_button_container->AddChildView(std::move(delete_button));
    AddChildView(std::move(delete_button_container));

    return delete_button_ptr;
  }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    UpdateImageViewSize();
  }

  void OnThemeChanged() override {
    // Use the light mode as default because the light mode is the default mode
    // of the native theme which decides the context menu's background color.
    // TODO(andrewxu): remove this line after https://crbug.com/1143009 is
    // fixed.
    ScopedLightModeAsDefault scoped_light_mode_as_default;

    ContentsView::OnThemeChanged();
    border_container_view_->border()->set_color(
        AshColorProvider::Get()->GetControlsLayerColor(
            AshColorProvider::ControlsLayerType::kHairlineBorderColor));
  }

  std::unique_ptr<RoundedImageView> BuildImageView() {
    // `BuildImageView()` achieves the image's rounded corners through
    // RoundedImageView instead of layer. Because the menu's container does not
    // cut the children's layers outside of the container's bounds. As a result,
    // if menu items have their own layers, the part beyond the container's
    // bounds is still visible when the context menu is in overflow.

    const auto* clipboard_history_item = container_->clipboard_history_item();
    switch (container_->data_format_) {
      case ui::ClipboardInternalFormat::kHtml:
        return std::make_unique<FadeImageView>(
            clipboard_history_item, container_->resource_manager_,
            base::BindRepeating(&BitmapContentsView::UpdateImageViewSize,
                                weak_ptr_factory_.GetWeakPtr()));
      case ui::ClipboardInternalFormat::kBitmap: {
        auto image_view = std::make_unique<RoundedImageView>(
            ClipboardHistoryViews::kImageRoundedCornerRadius,
            RoundedImageView::Alignment::kCenter);
        gfx::ImageSkia bitmap_image = gfx::ImageSkia::CreateFrom1xBitmap(
            clipboard_history_item->data().bitmap());
        image_view->SetImage(bitmap_image);
        return image_view;
      }
      default:
        NOTREACHED();
        return nullptr;
    }
  }

  void UpdateImageViewSize() {
    const gfx::Size image_size = image_view_->original_image().size();
    gfx::Rect contents_bounds = GetContentsBounds();

    const float width_ratio =
        image_size.width() / float(contents_bounds.width());
    const float height_ratio =
        image_size.height() / float(contents_bounds.height());

    // Calculate `scaling_up_ratio` depending on the image type. A bitmap image
    // should fill the contents bounds while an image rendered from HTML
    // should meet at least one edge of the contents bounds.
    float scaling_up_ratio = 0.f;
    switch (container_->data_format_) {
      case ui::ClipboardInternalFormat::kBitmap: {
        if (width_ratio >= 1.f && height_ratio >= 1.f)
          scaling_up_ratio = 1.f;
        else
          scaling_up_ratio = std::fmin(width_ratio, height_ratio);
        DCHECK_LE(scaling_up_ratio, 1.f);
        break;
      }
      case ui::ClipboardInternalFormat::kHtml: {
        scaling_up_ratio = std::fmax(width_ratio, height_ratio);
        break;
      }
      default:
        NOTREACHED();
        break;
    }

    DCHECK_GT(scaling_up_ratio, 0.f);

    image_view_->SetImage(image_view_->original_image(),
                          gfx::Size(image_size.width() / scaling_up_ratio,
                                    image_size.height() / scaling_up_ratio));
  }

  ClipboardHistoryBitmapItemView* const container_;
  RoundedImageView* image_view_ = nullptr;

  // Helps to place a border above `image_view_`.
  views::View* border_container_view_ = nullptr;

  base::WeakPtrFactory<BitmapContentsView> weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryBitmapItemView

ClipboardHistoryBitmapItemView::ClipboardHistoryBitmapItemView(
    const ClipboardHistoryItem* clipboard_history_item,
    const ClipboardHistoryResourceManager* resource_manager,
    views::MenuItemView* container)
    : ClipboardHistoryItemView(clipboard_history_item, container),
      resource_manager_(resource_manager),
      data_format_(*ClipboardHistoryUtil::CalculateMainFormat(
          clipboard_history_item->data())) {}

ClipboardHistoryBitmapItemView::~ClipboardHistoryBitmapItemView() = default;

const char* ClipboardHistoryBitmapItemView::GetClassName() const {
  return "ClipboardHistoryBitmapItemView";
}

std::unique_ptr<ClipboardHistoryBitmapItemView::ContentsView>
ClipboardHistoryBitmapItemView::CreateContentsView() {
  return std::make_unique<BitmapContentsView>(this);
}

base::string16 ClipboardHistoryBitmapItemView::GetAccessibleName() const {
  switch (data_format_) {
    case ui::ClipboardInternalFormat::kHtml:
      return l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_MENU_HTML_IMAGE);
    case ui::ClipboardInternalFormat::kBitmap:
      return l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_MENU_BITMAP_IMAGE);
    default:
      NOTREACHED();
      return base::string16();
  }
}

}  // namespace ash
