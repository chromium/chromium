// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/style/ash_color_id.h"
#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
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
// An `ImageView` which reacts to updates from its `ClipboardHistoryItem` by
// fading out the old image and fading in the new image. Used when HTML is done
// rendering. Expected to transition at most once in its lifetime.
class FadeImageView : public views::ImageView,
                      public ui::ImplicitAnimationObserver {
 public:
  FadeImageView(
      base::RepeatingCallback<const ClipboardHistoryItem*()> item_resolver,
      base::RepeatingClosure update_callback)
      : item_resolver_(item_resolver), update_callback_(update_callback) {
    CHECK(item_resolver_);
    CHECK(update_callback_);

    const auto* item = item_resolver_.Run();
    CHECK(item);
    // Subscribe to be notified when `item`'s display image updates.
    // `Unretained(this)` is safe because `this` owns the callback, and `item`
    // will not notify `this` of display image changes if
    // `display_image_updated_subscription_` is destroyed.
    display_image_updated_subscription_ =
        item->AddDisplayImageUpdatedCallback(base::BindRepeating(
            &FadeImageView::OnDisplayImageUpdated, base::Unretained(this)));

    SetImageFromModel();
  }

  FadeImageView(const FadeImageView& rhs) = delete;

  FadeImageView& operator=(const FadeImageView& rhs) = delete;

  ~FadeImageView() override { StopObservingImplicitAnimations(); }

  void OnDisplayImageUpdated() {
    // Fade the old image out, then swap in the new image.
    CHECK_EQ(FadeAnimationState::kNoFadeAnimation, animation_state_);
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
        CHECK_EQ(layer()->opacity(), 0.0f);
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
    if (const auto* item = item_resolver_.Run()) {
      CHECK(item->display_image().has_value());
      SetImage(item->display_image().value());
    }

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

  // Generates a *possibly null* pointer to the clipboard history item
  // represented by this image.
  base::RepeatingCallback<const ClipboardHistoryItem*()> item_resolver_;

  // Used to notify the contents view of image changes.
  base::RepeatingClosure update_callback_;

  // Subscription notified when the clipboard history item's image changes.
  base::CallbackListSubscription display_image_updated_subscription_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryBitmapItemView::BitmapContentsView

class ClipboardHistoryBitmapItemView::BitmapContentsView : public views::View {
 public:
  METADATA_HEADER(BitmapContentsView);
  explicit BitmapContentsView(ClipboardHistoryBitmapItemView* container)
      : container_(container) {
    views::Builder<views::View>(this)
        .SetLayoutManager(std::make_unique<views::FillLayout>())
        .AddChild(
            views::Builder<views::ImageView>(BuildImageView())
                .CopyAddressTo(&image_view_)
                .SetPreferredSize(gfx::Size(
                    INT_MAX, ClipboardHistoryViews::kImageViewPreferredHeight)))
        .BuildChildren();

    if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
      // Distinguish the image from rest of the menu with a colored background.
      SetBackground(views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysSeparator,
          ClipboardHistoryViews::kImageBackgroundCornerRadius));
    } else {
      // Distinguish the image from rest of the menu with a border.
      views::Builder<views::View>(this)
          .AddChild(views::Builder<views::View>().SetBorder(
              views::CreateThemedRoundedRectBorder(
                  ClipboardHistoryViews::kImageBorderThickness,
                  ClipboardHistoryViews::kImageBorderCornerRadius,
                  kColorAshHairlineBorderColor)))
          .BuildChildren();
    }
  }
  BitmapContentsView(const BitmapContentsView& rhs) = delete;
  BitmapContentsView& operator=(const BitmapContentsView& rhs) = delete;
  ~BitmapContentsView() override = default;

 private:
  // ContentsView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    // Create rounded corners around the contents area through the clip path
    // instead of layer clip. Because we have to avoid using any layer here.
    // Note that the menu's container does not cut the children's layers outside
    // of the container's bounds. As a result, if menu items have their own
    // layers, the part beyond the container's bounds is still visible when the
    // context menu is in overflow.
    const SkRect local_bounds = gfx::RectToSkRect(GetContentsBounds());
    const SkScalar radius =
        SkIntToScalar(chromeos::features::IsClipboardHistoryRefreshEnabled()
                          ? ClipboardHistoryViews::kImageBackgroundCornerRadius
                          : ClipboardHistoryViews::kImageBorderCornerRadius);
    SetClipPath(SkPath::RRect(local_bounds, radius, radius));

    UpdateImageViewSize();
  }

  std::unique_ptr<views::ImageView> BuildImageView() {
    const auto* clipboard_history_item = container_->GetClipboardHistoryItem();
    CHECK(clipboard_history_item);
    return std::make_unique<FadeImageView>(
        // `Unretained()` is safe because `this` owns the `FadeImageView` being
        // created, and `container_` owns `this`.
        base::BindRepeating(
            &ClipboardHistoryBitmapItemView::GetClipboardHistoryItem,
            base::Unretained(container_)),
        base::BindRepeating(&BitmapContentsView::UpdateImageViewSize,
                            base::Unretained(this)));
  }

  void UpdateImageViewSize() {
    if (chromeos::features::IsClipboardHistoryRefreshEnabled() &&
        image_view_->GetImageModel() ==
            clipboard_history_util::GetHtmlPreviewPlaceholder()) {
      // The bitmap item placeholder icon's size does not depend on the
      // available space.
      image_view_->SetImageSize(
          gfx::Size(ClipboardHistoryViews::kBitmapItemPlaceholderIconSize,
                    ClipboardHistoryViews::kBitmapItemPlaceholderIconSize));
      return;
    }

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

    CHECK_GT(scaling_up_ratio, 0.f);

    image_view_->SetImageSize(
        gfx::Size(image_size.width() / scaling_up_ratio,
                  image_size.height() / scaling_up_ratio));
  }

  const raw_ptr<ClipboardHistoryBitmapItemView, ExperimentalAsh> container_;
  raw_ptr<views::ImageView, ExperimentalAsh> image_view_ = nullptr;
};

BEGIN_METADATA(ClipboardHistoryBitmapItemView, BitmapContentsView, views::View)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryBitmapItemView

ClipboardHistoryBitmapItemView::ClipboardHistoryBitmapItemView(
    const base::UnguessableToken& item_id,
    const ClipboardHistory* clipboard_history,
    views::MenuItemView* container)
    : ClipboardHistoryItemView(item_id, clipboard_history, container),
      data_format_(GetClipboardHistoryItem()->main_format()) {
  switch (data_format_) {
    case ui::ClipboardInternalFormat::kHtml:
      SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_MENU_HTML_IMAGE));
      break;
    case ui::ClipboardInternalFormat::kPng:
      SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_MENU_PNG_IMAGE));
      break;
    default:
      NOTREACHED();
  }
}

ClipboardHistoryBitmapItemView::~ClipboardHistoryBitmapItemView() = default;

std::unique_ptr<views::View>
ClipboardHistoryBitmapItemView::CreateContentsView() {
  return std::make_unique<BitmapContentsView>(this);
}

BEGIN_METADATA(ClipboardHistoryBitmapItemView, ClipboardHistoryItemView)
END_METADATA

}  // namespace ash
