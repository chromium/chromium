// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_delete_button.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The duration of the fade out animation for transitioning the placeholder
// image to rendered HTML.
constexpr base::TimeDelta kFadeOutDurationMs = base::Milliseconds(60);

// The duration of the fade in animation for transitioning the placeholder image
// to rendered HTML.
constexpr base::TimeDelta kFadeInDurationMs = base::Milliseconds(200);

////////////////////////////////////////////////////////////////////////////////
// FadeImageView
// An ImageView which reacts to updates from ClipboardHistoryResourceManager by
// fading out the old image, and fading in the new image. Used when HTML is done
// rendering. Only expected to transition once in its lifetime.
class FadeImageView : public views::ImageView,
                      public ui::ImplicitAnimationObserver,
                      public ClipboardHistoryResourceManager::Observer {
 public:
  FadeImageView(const ClipboardHistoryItem* clipboard_history_item,
                const ClipboardHistoryResourceManager* resource_manager,
                base::RepeatingClosure update_callback)
      : views::ImageView(),
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

    border_container_view_->SetBorder(views::CreateThemedRoundedRectBorder(
        ClipboardHistoryViews::kImageBorderThickness,
        ClipboardHistoryViews::kImageRoundedCornerRadius,
        kColorAshHairlineBorderColor));

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
    // Create rounded corners around the contents area through the clip path
    // instead of layer clip. Because we have to avoid using any layer here.
    // Note that the menu's container does not cut the children's layers outside
    // of the container's bounds. As a result, if menu items have their own
    // layers, the part beyond the container's bounds is still visible when the
    // context menu is in overflow.
    const SkRect local_bounds = gfx::RectToSkRect(GetContentsBounds());
    const SkScalar radius =
        SkIntToScalar(ClipboardHistoryViews::kImageRoundedCornerRadius);
    SetClipPath(SkPath::RRect(local_bounds, radius, radius));

    UpdateImageViewSize();
  }

  std::unique_ptr<views::ImageView> BuildImageView() {
    const auto* clipboard_history_item = container_->clipboard_history_item();
    switch (container_->data_format_) {
      case ui::ClipboardInternalFormat::kHtml:
        return std::make_unique<FadeImageView>(
            clipboard_history_item, container_->resource_manager_,
            base::BindRepeating(&BitmapContentsView::UpdateImageViewSize,
                                weak_ptr_factory_.GetWeakPtr()));
      case ui::ClipboardInternalFormat::kPng: {
        auto image_view = std::make_unique<views::ImageView>();
        gfx::Image image;
        const auto& maybe_png = clipboard_history_item->data().maybe_png();
        if (maybe_png.has_value()) {
          image = gfx::Image::CreateFrom1xPNGBytes(maybe_png.value().data(),
                                                   maybe_png.value().size());
        } else {
          // If we have not yet encoded the bitmap to a PNG, just create the
          // gfx::Image using the available bitmap. No information is lost here.
          auto maybe_bitmap =
              clipboard_history_item->data().GetBitmapIfPngNotEncoded();
          DCHECK(maybe_bitmap.has_value());
          image = gfx::Image::CreateFrom1xBitmap(maybe_bitmap.value());
        }
        ui::ImageModel image_model = ui::ImageModel::FromImage(image);
        image_view->SetImage(image_model);
        return image_view;
      }
      default:
        NOTREACHED();
        return nullptr;
    }
  }

  void UpdateImageViewSize() {
    const gfx::Size image_size = image_view_->GetImage().size();
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
      case ui::ClipboardInternalFormat::kPng: {
        scaling_up_ratio = std::fmin(width_ratio, height_ratio);
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

    image_view_->SetImageSize(
        gfx::Size(image_size.width() / scaling_up_ratio,
                  image_size.height() / scaling_up_ratio));
  }

  ClipboardHistoryBitmapItemView* const container_;
  views::ImageView* image_view_ = nullptr;

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
      data_format_(*clipboard_history_util::CalculateMainFormat(
          clipboard_history_item->data())) {}

ClipboardHistoryBitmapItemView::~ClipboardHistoryBitmapItemView() = default;

const char* ClipboardHistoryBitmapItemView::GetClassName() const {
  return "ClipboardHistoryBitmapItemView";
}

std::unique_ptr<ClipboardHistoryBitmapItemView::ContentsView>
ClipboardHistoryBitmapItemView::CreateContentsView() {
  return std::make_unique<BitmapContentsView>(this);
}

std::u16string ClipboardHistoryBitmapItemView::GetAccessibleName() const {
  switch (data_format_) {
    case ui::ClipboardInternalFormat::kHtml:
      return l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_MENU_HTML_IMAGE);
    case ui::ClipboardInternalFormat::kPng:
      return l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_MENU_PNG_IMAGE);
    default:
      NOTREACHED();
      return std::u16string();
  }
}

}  // namespace ash
