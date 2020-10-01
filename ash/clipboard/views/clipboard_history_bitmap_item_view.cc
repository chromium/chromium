// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_bitmap_item_view.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The preferred height for the bitmap.
constexpr int kBitmapHeight = 64;

// The margins of the delete button.
constexpr gfx::Insets kDeleteButtonMargins =
    gfx::Insets(/*top=*/4, /*left=*/0, /*bottom=*/0, /*right=*/4);

// The duration of the fade out animation for transitioning the placeholder
// image to rendered HTML.
constexpr base::TimeDelta kFadeOutDurationMs =
    base::TimeDelta::FromMilliseconds(60);

// The duration of the fade in animation for transitioning the placeholder image
// to rendered HTML.
constexpr base::TimeDelta kFadeInDurationMs =
    base::TimeDelta::FromMilliseconds(200);

// The radius of the image's rounded corners.
constexpr int kRoundedCornerRadius = 4;

////////////////////////////////////////////////////////////////////////////////
// FadeImageView
// An ImageView which reacts to updates from ClipboardHistoryResourceManager by
// fading out the old image, and fading in the new image. Used when HTML is done
// rendering. Only expected to transition once in its lifetime.
class FadeImageView : public RoundedImageView,
                      public ui::ImplicitAnimationObserver,
                      public ClipboardHistoryResourceManager::Observer {
 public:
  FadeImageView(ClipboardHistoryBitmapItemView* bitmap_item_view,
                const ClipboardHistoryItem* clipboard_history_item,
                const ClipboardHistoryResourceManager* resource_manager,
                float opacity)
      : RoundedImageView(kRoundedCornerRadius),
        bitmap_item_view_(bitmap_item_view),
        resource_manager_(resource_manager),
        clipboard_history_item_(*clipboard_history_item),
        opacity_(opacity) {
    resource_manager_->AddObserver(this);
    SetImageFromModel();
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
    if (opacity_ != 1.f) {
      SetImage(
          gfx::ImageSkiaOperations::CreateTransparentImage(image, opacity_));
    } else {
      SetImage(image);
    }

    // When fading in a new image, the ImageView's image has likely changed
    // sizes.
    if (animation_state_ == FadeAnimationState::kFadeIn)
      bitmap_item_view_->UpdateChildImageViewSize();
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

  // The parent ClipboardHistoryBitmapItemView, used to notify of image changes.
  // Owned by the view hierarchy.
  ClipboardHistoryBitmapItemView* const bitmap_item_view_;

  // The resource manager, owned by ClipboardHistoryController.
  const ClipboardHistoryResourceManager* const resource_manager_;

  // The ClipboardHistoryItem represented by this class.
  const ClipboardHistoryItem clipboard_history_item_;

  // The opacity of the image content.
  const float opacity_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryBitmapItemView::BitmapContentsView

class ClipboardHistoryBitmapItemView::BitmapContentsView
    : public ClipboardHistoryBitmapItemView::ContentsView {
 public:
  explicit BitmapContentsView(ClipboardHistoryBitmapItemView* container)
      : ContentsView(container) {}
  BitmapContentsView(const BitmapContentsView& rhs) = delete;
  BitmapContentsView& operator=(const BitmapContentsView& rhs) = delete;
  ~BitmapContentsView() override = default;

  // ContentsView:
  DeleteButton* CreateDeleteButton() override {
    auto delete_button_container = std::make_unique<views::View>();
    auto* layout_manager = delete_button_container->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout_manager->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kEnd);
    layout_manager->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    auto delete_button = std::make_unique<DeleteButton>(container_);
    delete_button->SetVisible(false);
    delete_button->SetProperty(views::kMarginsKey, kDeleteButtonMargins);
    DeleteButton* delete_button_ptr =
        delete_button_container->AddChildView(std::move(delete_button));
    AddChildView(std::move(delete_button_container));

    return delete_button_ptr;
  }
};

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryBitmapItemView

ClipboardHistoryBitmapItemView::ClipboardHistoryBitmapItemView(
    const ClipboardHistoryItem* clipboard_history_item,
    const ClipboardHistoryResourceManager* resource_manager,
    views::MenuItemView* container)
    : ClipboardHistoryItemView(clipboard_history_item, container),
      resource_manager_(resource_manager) {}

ClipboardHistoryBitmapItemView::~ClipboardHistoryBitmapItemView() = default;

void ClipboardHistoryBitmapItemView::UpdateChildImageViewSize() {
  const gfx::Size image_size = image_view_->original_image().size();
  const double width_ratio = image_size.width() / double(width());
  const double height_ratio = image_size.height() / double(height());

  if (width_ratio <= 1.f || height_ratio <= 1.f) {
    image_view_->SetImage(image_view_->original_image(), image_size);
    return;
  }

  const double resize_ratio = std::fmin(width_ratio, height_ratio);
  image_view_->SetImage(image_view_->original_image(),
                        gfx::Size(image_size.width() / resize_ratio,
                                  image_size.height() / resize_ratio));
}

const char* ClipboardHistoryBitmapItemView::GetClassName() const {
  return "ClipboardHistoryBitmapItemView";
}

std::unique_ptr<ClipboardHistoryBitmapItemView::ContentsView>
ClipboardHistoryBitmapItemView::CreateContentsView() {
  auto contents_view = std::make_unique<BitmapContentsView>(this);
  contents_view->SetLayoutManager(std::make_unique<views::FillLayout>());

  auto image_view = BuildImageView();
  image_view->SetPreferredSize(gfx::Size(INT_MAX, kBitmapHeight));
  image_view_ = contents_view->AddChildView(std::move(image_view));
  contents_view->InstallDeleteButton();
  return contents_view;
}

void ClipboardHistoryBitmapItemView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  UpdateChildImageViewSize();
}

std::unique_ptr<RoundedImageView>
ClipboardHistoryBitmapItemView::BuildImageView() {
  // `BuildImageView()` achieves the image's rounded corners through
  // RoundedImageView instead of layer. Because the menu's container does not
  // cut the children's layers outside of the container's bounds. As a result,
  // if menu items have their own layers, the part beyond the container's bounds
  // is still visible when the context menu is in overflow.

  switch (ClipboardHistoryUtil::CalculateMainFormat(
              clipboard_history_item()->data())
              .value()) {
    case ui::ClipboardInternalFormat::kHtml:
      return std::make_unique<FadeImageView>(this, clipboard_history_item(),
                                             resource_manager_,
                                             GetContentsOpacity());
    case ui::ClipboardInternalFormat::kBitmap: {
      auto image_view =
          std::make_unique<RoundedImageView>(kRoundedCornerRadius);
      gfx::ImageSkia bitmap_image = gfx::ImageSkia::CreateFrom1xBitmap(
          clipboard_history_item()->data().bitmap());
      if (GetContentsOpacity() != 1.f) {
        bitmap_image = gfx::ImageSkiaOperations::CreateTransparentImage(
            bitmap_image, GetContentsOpacity());
      }
      image_view->SetImage(bitmap_image);
      return image_view;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace ash
