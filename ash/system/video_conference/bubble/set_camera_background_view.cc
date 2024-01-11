// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/set_camera_background_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/video_conference/bubble/bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

namespace ash::video_conference {

namespace {

using BackgroundImageInfo = CameraEffectsController::BackgroundImageInfo;

// Decides the margin for the `SetCameraBackgroundView`.
constexpr gfx::Insets kSetCameraBackgroundViewInsideBorderInsets =
    gfx::Insets::TLBR(16, 4, 0, 4);

// This extra border is added to `CreateImageButton` to make it consistent with
// other buttons in the video conference bubble.
constexpr gfx::Insets kCreateImageButtonBorderInsets = gfx::Insets::VH(8, 0);

constexpr int kCreateImageButtonBetweenChildSpacing = 16;
constexpr int kSetCameraBackgroundViewBetweenChildSpacing = 8;
constexpr int kSetCameraBackgroundViewRadius = 16;
constexpr int kButtonHeight = 20;

constexpr int kMaxRecentBackgroundToDislay = 5;
constexpr int kRecentlyUsedImagesFullLength = 328;
constexpr int kRecentlyUsedImagesHeight = 64;
constexpr int kRecentlyUsedImagesSpacing = 8;

// Helper for getting the width of each recently used images.
int GetRecentlyUsedImageWidth(const int index, const int image_count) {
  CHECK_LT(index, image_count);

  // The first image should be handled separately.
  if (index == 0) {
    // The first image takes full length if it is the only image. Otherwise, it
    // takes exactly half of the length.
    return image_count == 1
               ? kRecentlyUsedImagesFullLength
               : (kRecentlyUsedImagesFullLength - kRecentlyUsedImagesSpacing) /
                     2;
  }

  // The rest of the image should share the rest of the space evenly.
  // The length left for the rest of the images besides the first one.
  const int length_after_the_first =
      (kRecentlyUsedImagesFullLength - kRecentlyUsedImagesSpacing) / 2;
  // The rest `image_count` - 1 images should have `image_count` -2 gaps between
  // them.
  const int spacing_for_the_rest_images =
      (image_count - 2) * kRecentlyUsedImagesSpacing;
  const int length_per_image =
      (length_after_the_first - spacing_for_the_rest_images) /
      (image_count - 1);

  return length_per_image;
}

CameraEffectsController* GetCameraEffectsController() {
  return Shell::Get()->camera_effects_controller();
}

// Resize the `bitmap` (keeping its ratio) to just cover the `expected_size`;
// then crop the extra bit that is outside of the `expected_size`.
gfx::ImageSkia ConstrainedScaleAndCrop(const SkBitmap& bitmap,
                                       const gfx::Size& expected_size) {
  const int bitmap_height = bitmap.height();
  const int bitmap_width = bitmap.width();
  const int expected_height = expected_size.height();
  const int expected_width = expected_size.width();

  // We need to scale to the larger ratio so the image can still fully cover the
  // expected_size.
  const float ratio = std::max(
      static_cast<float>(expected_height) / static_cast<float>(bitmap_height),
      static_cast<float>(expected_width) / static_cast<float>(bitmap_width));

  const auto new_size =
      gfx::ScaleToCeiledSize(gfx::Size(bitmap_width, bitmap_height), ratio);

  // Resize and only take the expected_size.
  auto resized = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_LANCZOS3, new_size.width(),
      new_size.height(), SkIRect::MakeWH(expected_width, expected_height));

  return gfx::ImageSkia::CreateFrom1xBitmap(resized);
}

// Image button for the recently used images as camera background.
class RecentlyUsedImageButton : public views::ImageButton {
  METADATA_HEADER(RecentlyUsedImageButton, views::ImageButton)

 public:
  RecentlyUsedImageButton(const base::FilePath& filename,
                          const std::string& jpeg_bytes,
                          const int expected_width)
      : ImageButton(
            base::BindRepeating(&RecentlyUsedImageButton::OnButtonClicked,
                                base::Unretained(this))),
        filename_(filename) {
    // Deccode the jpeg content.
    std::unique_ptr<SkBitmap> bitmap = gfx::JPEGCodec::Decode(
        reinterpret_cast<const unsigned char*>(jpeg_bytes.data()),
        jpeg_bytes.size());

    // Resize to the right size.
    const auto resized = ConstrainedScaleAndCrop(
        *bitmap, gfx::Size(expected_width, kRecentlyUsedImagesHeight));

    // Add round corner.
    const auto img = gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
        kSetCameraBackgroundViewRadius, resized);

    // Set as background image.
    SetImageModel(ButtonState::STATE_NORMAL,
                  ui::ImageModel::FromImageSkia(img));
  }

  // Apply background replace when the button is clicked on.
  void OnButtonClicked(const ui::Event& event) {
    GetCameraEffectsController()->SetBackgroundImage(filename_,
                                                     base::DoNothing());
  }

 private:
  const base::FilePath filename_;
};

BEGIN_METADATA(RecentlyUsedImageButton)
END_METADATA

// The RecentlyUsedBackgroundView contains a list of recently used background
// images as buttons.
class RecentlyUsedBackgroundView : public views::View {
  METADATA_HEADER(RecentlyUsedBackgroundView, views::View)

 public:
  explicit RecentlyUsedBackgroundView(BubbleView* bubble_view)
      : bubble_view_(bubble_view) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        /*inside_border_insets=*/gfx::Insets(),
        /*between_child_spacing=*/kRecentlyUsedImagesSpacing));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);

    GetCameraEffectsController()->GetRecentlyUsedBackgroundImages(
        kMaxRecentBackgroundToDislay,
        base::BindOnce(&RecentlyUsedBackgroundView::
                           GetRecentlyUsedBackgroundImagesComplete,
                       weak_factory_.GetWeakPtr()));
  }

  void GetRecentlyUsedBackgroundImagesComplete(
      const std::vector<BackgroundImageInfo>& images_info) {
    for (std::size_t i = 0; i < images_info.size(); ++i) {
      AddChildView(std::make_unique<RecentlyUsedImageButton>(
          images_info[i].basename, images_info[i].jpeg_bytes,
          GetRecentlyUsedImageWidth(i, images_info.size())));
    }

    // Because this is async, we need to update the ui when all images are
    // loaded.
    if (bubble_view_) {
      bubble_view_->ChildPreferredSizeChanged(this);
    }
  }

 private:
  raw_ptr<BubbleView> bubble_view_;

  base::WeakPtrFactory<RecentlyUsedBackgroundView> weak_factory_{this};
};

BEGIN_METADATA(RecentlyUsedBackgroundView)
END_METADATA

// Button for "Create with AI".
class CreateImageButton : public views::LabelButton {
  METADATA_HEADER(CreateImageButton, views::LabelButton)

 public:
  CreateImageButton(VideoConferenceTrayController* controller)
      : views::LabelButton(
            base::BindRepeating(&CreateImageButton::OnButtonClicked,
                                base::Unretained(this)),
            l10n_util::GetStringUTF16(
                IDS_ASH_VIDEO_CONFERENCE_CREAT_WITH_AI_NAME)),
        controller_(controller) {
    SetBorder(views::CreateEmptyBorder(kCreateImageButtonBorderInsets));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    SetImageLabelSpacing(kCreateImageButtonBetweenChildSpacing);
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase, kSetCameraBackgroundViewRadius));
    SetImageModel(ButtonState::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      kAiWandIcon, ui::kColorMenuIcon, kButtonHeight));
  }

  CreateImageButton(const CreateImageButton&) = delete;
  CreateImageButton& operator=(const CreateImageButton&) = delete;
  ~CreateImageButton() override = default;

 private:
  void OnButtonClicked(const ui::Event& event) {
    controller_->CreateBackgroundImage();
  }

  // Unowned by `CreateImageButton`.
  const raw_ptr<VideoConferenceTrayController> controller_;
};

BEGIN_METADATA(CreateImageButton)
END_METADATA

}  // namespace

SetCameraBackgroundView::SetCameraBackgroundView(
    BubbleView* bubble_view,
    VideoConferenceTrayController* controller) {
  SetID(BubbleViewID::kSetCameraBackgroundView);

  // `SetCameraBackgroundView` has 2+ children, we want to stack them
  // vertically.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/kSetCameraBackgroundViewInsideBorderInsets,
      /*between_child_spacing=*/kSetCameraBackgroundViewBetweenChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  AddChildView(std::make_unique<RecentlyUsedBackgroundView>(bubble_view));
  AddChildView(std::make_unique<CreateImageButton>(controller));
}

BEGIN_METADATA(SetCameraBackgroundView)
END_METADATA

}  // namespace ash::video_conference
