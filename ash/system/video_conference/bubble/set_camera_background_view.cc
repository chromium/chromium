// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/video_conference/bubble/set_camera_background_view.h"

#include "ash/public/cpp/image_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/video_conference/bubble/bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/resources/grit/vc_resources.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash::video_conference {

namespace {

using BackgroundImageInfo = CameraEffectsController::BackgroundImageInfo;

constexpr char kBackgroundImageButtonHistogramName[] =
    "Ash.VideoConferenceTray.BackgroundImageButton.Click";

constexpr char kCreateWithAiButtonHistogramName[] =
    "Ash.VideoConferenceTray.CreateWithAiButton.Click";

// This extra border is added to `CreateImageButton` to make it consistent with
// other buttons in the video conference bubble.
constexpr gfx::Insets kImageLabelContainerInsideBorderInsets =
    gfx::Insets::VH(8, 0);

// Distance between the wand icon and the "Create with AI" label
constexpr int kCreateImageButtonBetweenChildSpacing = 12;
// Vertical Distance between Recently used image and Create with AI button
constexpr int kSetCameraBackgroundViewBetweenChildSpacing = 16;
constexpr int kSetCameraBackgroundViewRadius = 18;
constexpr int kButtonHeight = 20;

constexpr int kMaxRecentBackgroundToDislay = 4;
constexpr int kRecentlyUsedImagesFullLength = 368;
constexpr int kRecentlyUsedImagesHeight = 76;
constexpr int kRecentlyUsedImagesSpacing = 10;

constexpr int kRecentlyUsedImageButtonId[] = {
    BubbleViewID::kBackgroundImage0,
    BubbleViewID::kBackgroundImage1,
    BubbleViewID::kBackgroundImage2,
    BubbleViewID::kBackgroundImage3,
};

// Helper for getting the size of each recently used images.
gfx::Size CalculateWantedImageSize(const int index, int image_count) {
  CHECK_LT(index, image_count);

  const int spacing = (image_count - 1) * kRecentlyUsedImagesSpacing;

  // If there is only 1 image, we only want that image to take half of the whole
  // area, not the full area.
  const int expected_width =
      image_count == 1
          ? (kRecentlyUsedImagesFullLength - kRecentlyUsedImagesSpacing) / 2
          : (kRecentlyUsedImagesFullLength - spacing) / image_count;

  return gfx::Size(expected_width, kRecentlyUsedImagesHeight);
}

CameraEffectsController* GetCameraEffectsController() {
  return Shell::Get()->camera_effects_controller();
}

// Returns a gradient lottie animation defined in the resource file for the
// `Create with AI` button.
std::unique_ptr<lottie::Animation> GetGradientAnimation(
    const ui::ColorProvider* color_provider) {
  std::optional<std::vector<uint8_t>> lottie_data =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(
          IDR_VC_CREATE_WITH_AI_BUTTON_ANIMATION);
  CHECK(lottie_data.has_value());
  CHECK(color_provider);

  return std::make_unique<lottie::Animation>(
      cc::SkottieWrapper::UnsafeCreateSerializable(lottie_data.value()),
      video_conference_utils::CreateColorMapForGradientAnimation(
          color_provider));
}

// Image button for the recently used images as camera background.
class RecentlyUsedImageButton : public views::ImageButton {
  METADATA_HEADER(RecentlyUsedImageButton, views::ImageButton)

 public:
  RecentlyUsedImageButton(
      const gfx::ImageSkia& image,
      const std::string& metadata,
      int id,
      const base::RepeatingCallback<void()>& image_button_callback)
      : ImageButton(image_button_callback),
        check_icon_(&kBackgroundSelectionIcon,
                    cros_tokens::kCrosSysFocusRingOnPrimaryContainer) {
    SetID(id);
    background_image_ = image;

    SetImageModel(ButtonState::STATE_NORMAL,
                  ui::ImageModel::FromImageSkia(background_image_));

    // TODO(b/332573200): only construct this button when the metadata is
    // decodable.
    SetAccessibilityLabelFromMetadata(metadata);

    SetFlipCanvasOnPaintForRTLUI(false);
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

  // Extract medata then decode it.
  void SetAccessibilityLabelFromMetadata(const std::string& metadata) {
    // Used for testing.
    if (metadata.empty()) {
      SetAccessibilityLabelFromRecentSeaPenImageInfo(nullptr);
      return;
    }

    const std::string extracted_metadata =
        ExtractDcDescriptionContents(metadata);

    DecodeJsonMetadata(
        extracted_metadata.empty() ? metadata : extracted_metadata,
        base::BindOnce(&RecentlyUsedImageButton::
                           SetAccessibilityLabelFromRecentSeaPenImageInfo,
                       weak_factory_.GetWeakPtr()));
  }

  // Called when decoding metadata complete.
  void SetAccessibilityLabelFromRecentSeaPenImageInfo(
      personalization_app::mojom::RecentSeaPenImageInfoPtr info) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
    GetViewAccessibility().SetDescription(l10n_util::GetStringUTF16(
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_IMAGE_LIST_ITEM_DESCRIPTION));

    std::u16string query;
    const auto& text = GetQueryString(info);
    if (text.empty() || !base::UTF8ToUTF16(text.c_str(), text.size(), &query)) {
      query.clear();
    }
    GetViewAccessibility().SetName(
        query, query.empty() ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                             : ax::mojom::NameFrom::kAttribute);
  }

  SkPath GetClipPath() {
    const auto width = this->width();
    const auto height = this->height();
    const auto radius = kSetCameraBackgroundViewRadius;

    return SkPath()
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
        .close();
  }

  bool selected_ = false;
  const ui::ThemedVectorIcon check_icon_;

  base::WeakPtrFactory<RecentlyUsedImageButton> weak_factory_{this};
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
      auto image = image_util::ResizeAndCropImage(
          images_info[i].image,
          CalculateWantedImageSize(i, images_info.size()));

      image = gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
          kSetCameraBackgroundViewRadius, image);

      auto recently_used_image_button =
          std::make_unique<RecentlyUsedImageButton>(
              image, images_info[i].metadata, kRecentlyUsedImageButtonId[i],
              base::BindRepeating(
                  &RecentlyUsedBackgroundView::OnImageButtonClicked,
                  weak_factory_.GetWeakPtr(), i, images_info[i].basename));
      // If background replace is applied, then set first image as selected.
      if (i == 0 &&
          GetCameraEffectsController()->GetCameraEffects()->replace_enabled) {
        recently_used_image_button->SetSelected(true);
      }
      AddChildView(std::move(recently_used_image_button));
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

    base::UmaHistogramBoolean(kBackgroundImageButtonHistogramName, true);
  }

 private:
  raw_ptr<BubbleView> bubble_view_;

  base::WeakPtrFactory<RecentlyUsedBackgroundView> weak_factory_{this};
};

BEGIN_METADATA(RecentlyUsedBackgroundView)
END_METADATA

// Button for "Create with AI".
class CreateImageButton : public views::Button {
  METADATA_HEADER(CreateImageButton, views::Button)

 public:
  explicit CreateImageButton(VideoConferenceTrayController* controller)
      : views::Button(base::BindRepeating(&CreateImageButton::OnButtonClicked,
                                          base::Unretained(this))),
        controller_(controller) {
    SetID(BubbleViewID::kCreateWithAiButton);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_CREAT_WITH_AI_NAME));
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase, kSetCameraBackgroundViewRadius));

    lottie_animation_view_ =
        AddChildView(std::make_unique<views::AnimatedImageView>());

    AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetBetweenChildSpacing(kCreateImageButtonBetweenChildSpacing)
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
            .SetInsideBorderInsets(kImageLabelContainerInsideBorderInsets)
            .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
            .AddChildren(
                views::Builder<views::ImageView>().SetImage(
                    ui::ImageModel::FromVectorIcon(
                        kAiWandIcon, ui::kColorMenuIcon, kButtonHeight)),
                views::Builder<views::Label>().SetText(
                    l10n_util::GetStringUTF16(
                        IDS_ASH_VIDEO_CONFERENCE_CREAT_WITH_AI_NAME)))
            .Build());
  }

  CreateImageButton(const CreateImageButton&) = delete;
  CreateImageButton& operator=(const CreateImageButton&) = delete;
  ~CreateImageButton() override = default;

  bool IsAnimationPlaying() {
    return lottie_animation_view_->state() ==
           views::AnimatedImageView::State::kPlaying;
  }

 private:
  // views::Button:
  // Reset the animated image on theme changed to get correct color for the
  // animation if the `lottie_animation_view_` should be shown and is visible.
  void OnThemeChanged() override {
    views::Button::OnThemeChanged();

    // Reset the animated image if there is a need to show animation.
    if (controller_->ShouldShowCreateWithAiButtonAnimation()) {
      // This need to be recorded before SetAnimatedImage because
      // SetAnimatedImage stops the animation.
      const bool is_animation_playing = IsAnimationPlaying();

      lottie_animation_view_->SetAnimatedImage(
          GetGradientAnimation(GetColorProvider()));

      // Play the animation only if it is current visible.
      if (is_animation_playing) {
        PlayAnimation();
      }
    }
  }

  void VisibilityChanged(View* starting_from, bool is_visible) override {
    // Skip visibility change that caused by the buble. We only care the
    // visibility change that is directly caused by `SetCameraBackgroundView`.
    if (starting_from != parent() ||
        !controller_->ShouldShowCreateWithAiButtonAnimation()) {
      return;
    }

    if (is_visible) {
      PlayAnimation();
    } else {
      StopAnimation();
    }
  }

  void OnButtonClicked(const ui::Event& event) {
    if (IsAnimationPlaying()) {
      StopAnimation();
    }
    controller_->DismissCreateWithAiButtonAnimationForever();

    base::UmaHistogramBoolean(kCreateWithAiButtonHistogramName, true);

    // This line will dismiss the VcTray, thus should be called at the end.
    controller_->CreateBackgroundImage();
  }

  void PlayAnimation() {
    if (!lottie_animation_view_->animated_image()) {
      lottie_animation_view_->SetAnimatedImage(
          GetGradientAnimation(GetColorProvider()));
    }
    lottie_animation_view_->SetVisible(true);
    lottie_animation_view_->Play();
    stop_animation_timer_.Start(FROM_HERE, kGradientAnimationDuration, this,
                                &CreateImageButton::StopAnimation);
  }

  void StopAnimation() {
    stop_animation_timer_.Stop();
    lottie_animation_view_->Stop();
    lottie_animation_view_->SetVisible(false);
  }

  // Unowned by `CreateImageButton`.
  const raw_ptr<VideoConferenceTrayController> controller_;

  // Owned by the View's hierarchy. Used to play the animation on the button.
  raw_ptr<views::AnimatedImageView> lottie_animation_view_ = nullptr;

  // Started when `lottie_animation_view_` starts playing the animation. It's
  // used to stop the animation after the animation duration.
  base::OneShotTimer stop_animation_timer_;
};

BEGIN_METADATA(CreateImageButton)
END_METADATA

}  // namespace

SetCameraBackgroundView::SetCameraBackgroundView(
    BubbleView* bubble_view,
    VideoConferenceTrayController* controller)
    : controller_(controller) {
  SetID(BubbleViewID::kSetCameraBackgroundView);
  SetVisible(
      GetCameraEffectsController()->GetCameraEffects()->replace_enabled &&
      GetCameraEffectsController()->IsVcBackgroundAllowedByEnterprise());

  // `SetCameraBackgroundView` has 2+ children, we want to stack them
  // vertically.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical,
                       /*inside_border_insets=*/gfx::Insets(),
                       /*between_child_spacing=*/
                       kSetCameraBackgroundViewBetweenChildSpacing))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);

  recently_used_background_view_ =
      AddChildView(std::make_unique<RecentlyUsedBackgroundView>(bubble_view));
  create_with_image_button_ =
      AddChildView(std::make_unique<CreateImageButton>(controller));
}

void SetCameraBackgroundView::SetBackgroundReplaceUiVisible(bool visible) {
  // We don't want to show the SetCameraBackgroundView if there is no recently
  // used background; instead, the webui is shown.
  if (visible && recently_used_background_view_->children().empty()) {
    // We need to double check that there is no background images.
    GetCameraEffectsController()->GetRecentlyUsedBackgroundImages(
        1, base::BindOnce(
               &SetCameraBackgroundView::OnGetRecentlyUsedBackgroundImages,
               weak_factory_.GetWeakPtr()));
  }

  SetVisible(visible);

  // Unselect all recently image buttons if this view is invisible.
  if (!visible) {
    for (auto& button : recently_used_background_view_->children()) {
      views::AsViewClass<RecentlyUsedImageButton>(button)->SetSelected(false);
    }
  }
}

SetCameraBackgroundView::~SetCameraBackgroundView() = default;

bool SetCameraBackgroundView::
    IsAnimationPlayingForCreateWithAiButtonForTesting() {
  return views::AsViewClass<CreateImageButton>(create_with_image_button_)
      ->IsAnimationPlaying();
}

void SetCameraBackgroundView::OnGetRecentlyUsedBackgroundImages(
    const std::vector<BackgroundImageInfo>& background_images) {
  // Directly open the VcBackgroundApp if no background image exists.
  if (background_images.empty()) {
    controller_->CreateBackgroundImage();
  }
}

BEGIN_METADATA(SetCameraBackgroundView)
END_METADATA

}  // namespace ash::video_conference
