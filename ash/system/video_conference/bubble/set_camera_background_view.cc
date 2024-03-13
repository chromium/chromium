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
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
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
    gfx::Insets::TLBR(10, 0, 0, 0);

// This extra border is added to `CreateImageButton` to make it consistent with
// other buttons in the video conference bubble.
constexpr gfx::Insets kCreateImageButtonBorderInsets = gfx::Insets::VH(8, 0);

constexpr int kCreateImageButtonBetweenChildSpacing = 12;
constexpr int kSetCameraBackgroundViewBetweenChildSpacing = 10;
constexpr int kSetCameraBackgroundViewRadius = 16;
constexpr int kButtonHeight = 20;

constexpr int kMaxRecentBackgroundToDislay = 4;
constexpr int kRecentlyUsedImagesFullLength = 336;
constexpr int kRecentlyUsedImagesHeight = 64;
constexpr int kRecentlyUsedImagesSpacing = 10;

// Helper for getting the width of each recently used images.
int GetRecentlyUsedImageWidth(const int index, int image_count) {
  CHECK_LT(index, image_count);

  // If there is only 1 image, we only want that image to take half of the whole
  // area, not the full area.
  if (image_count == 1) {
    return (kRecentlyUsedImagesFullLength - kRecentlyUsedImagesSpacing) / 2;
  }

  const int spacing = (image_count - 1) * kRecentlyUsedImagesSpacing;

  return (kRecentlyUsedImagesFullLength - spacing) / image_count;
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

  // target_area is a cropped area from the center.
  const auto target_area =
      SkIRect::MakeXYWH((new_size.width() - expected_width) / 2,
                        (new_size.height() - expected_height) / 2,
                        expected_width, expected_height);

  // Resize and only take the expected_size.
  auto resized = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_LANCZOS3, new_size.width(),
      new_size.height(), target_area);

  return gfx::ImageSkia::CreateFrom1xBitmap(resized);
}

// Helper to resize the image and return as ImageSkia.
gfx::ImageSkia GetResizedBackground(const std::string& jpeg_bytes,
                                    const int expected_width) {
  // TODO(b/329324151): evaluate the cost of the decoding and consider moving
  // this to the io thread. The same code is also used in
  // VcBackgroundUISeaPenProviderImpl.
  std::unique_ptr<SkBitmap> bitmap = gfx::JPEGCodec::Decode(
      reinterpret_cast<const unsigned char*>(jpeg_bytes.data()),
      jpeg_bytes.size());

  const auto resized_ = ConstrainedScaleAndCrop(
      *bitmap, gfx::Size(expected_width, kRecentlyUsedImagesHeight));

  const auto img = gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
      kSetCameraBackgroundViewRadius, resized_);

  return img;
}

// Image button for the recently used images as camera background.
class RecentlyUsedImageButton : public views::ImageButton {
  METADATA_HEADER(RecentlyUsedImageButton, views::ImageButton)

 public:
  RecentlyUsedImageButton(
      const std::string& jpeg_bytes,
      const int expected_width,
      const base::RepeatingCallback<void()>& image_button_callback)
      : ImageButton(image_button_callback),
        check_icon_(&kBackgroundSelectionIcon,
                    cros_tokens::kCrosSysFocusRingOnPrimaryContainer) {
    background_image_ = GetResizedBackground(jpeg_bytes, expected_width);
    SetImageModel(ButtonState::STATE_NORMAL,
                  ui::ImageModel::FromImageSkia(background_image_));
  }

  void SetSelected(bool selected) {
    if (selected_ == selected) {
      return;
    }
    selected_ = selected;
    SchedulePaint();
  }

 private:
  // ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    // If current image is selected, we need to draw the background image with
    // required boundary and then put the check mark on the top-left corner.
    if (selected_) {
      canvas->DrawImageInPath(background_image_, 0, 0, GetClipPath(),
                              cc::PaintFlags());
      canvas->DrawImageInt(check_icon_.GetImageSkia(GetColorProvider()), 0, 0);
    } else {
      // Otherwise, draw the normal background image.
      canvas->DrawImageInt(background_image_, 0, 0);
    }
  }

  SkPath GetClipPath() {
    const auto width = this->width();
    const auto height = this->height();
    const auto radius = kSetCameraBackgroundViewRadius;

    return SkPathBuilder()
        // Start just before the curve of the top-right corner.
        .moveTo(width - radius, 0.f)
        // Move to left before the curve.
        .lineTo(38, 0)
        // Draw first part of the top-left corner.
        .rCubicTo(-5.52f, 0, -10, 4.48f, -10, 10)
        // Move down a bit.
        .rLineTo(-0.f, 2.f)
        // Draw second part of the top-left corner.
        .rCubicTo(0, 8.84f, -7.16f, 16, -16, 16)
        // Move left a bit.
        .rLineTo(-2.f, 0.f)
        // Draw the third part of the top-left corner.
        .cubicTo(4.48f, 28, 0, 32.48f, 0, 38)
        // Move to the bottom-left corner.
        .lineTo(-0.f, height - radius)
        // Draw bottom-left curve.
        .rCubicTo(0, 8.84f, 7.16f, 16, 16, 16)
        // Move to the bottom-right corner.
        .lineTo(width - radius, height)
        // Draw bottom-right curve.
        .rCubicTo(8.84f, 0, 16, -7.16f, 16, -16)
        // Move to the top-right corner.
        .lineTo(width, 16)
        // Draw top-right curve.
        .rCubicTo(0, -8.84f, -7.16f, -16, -16, -16)
        .close()
        .detach();
  }

  bool selected_ = false;
  const ui::ThemedVectorIcon check_icon_;
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
          images_info[i].jpeg_bytes,
          GetRecentlyUsedImageWidth(i, images_info.size()),
          base::BindRepeating(&RecentlyUsedBackgroundView::OnImageButtonClicked,
                              weak_factory_.GetWeakPtr(), i,
                              images_info[i].basename)));
    }

    // Because this is async, we need to update the ui when all images are
    // loaded.
    if (bubble_view_) {
      bubble_view_->ChildPreferredSizeChanged(this);
    }
  }

  // Called when index-th image button is clicked on.
  void OnImageButtonClicked(std::size_t index, const base::FilePath& filename) {
    // Select index-th image button and deselect the rest of the image buttons.
    for (std::size_t i = 0; i < children().size(); i++) {
      RecentlyUsedImageButton* button =
          views::AsViewClass<RecentlyUsedImageButton>(children()[i]);
      button->SetSelected(i == index);
    }

    GetCameraEffectsController()->SetBackgroundImage(filename,
                                                     base::DoNothing());
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
    VideoConferenceTrayController* controller)
    : controller_(controller) {
  SetID(BubbleViewID::kSetCameraBackgroundView);

  // `SetCameraBackgroundView` has 2+ children, we want to stack them
  // vertically.
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/kSetCameraBackgroundViewInsideBorderInsets,
      /*between_child_spacing=*/kSetCameraBackgroundViewBetweenChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  recently_used_background_view_ =
      AddChildView(std::make_unique<RecentlyUsedBackgroundView>(bubble_view));
  AddChildView(std::make_unique<CreateImageButton>(controller));
}

void SetCameraBackgroundView::SetBackgroundReplaceUiVisible(bool visible) {
  // We don't want to show the SetCameraBackgroundView if there is no recently
  // used background; instead, the webui is shown.
  if (visible && recently_used_background_view_->children().empty()) {
    controller_->CreateBackgroundImage();
    return;
  }

  SetVisible(visible);
}

BEGIN_METADATA(SetCameraBackgroundView)
END_METADATA

}  // namespace ash::video_conference
