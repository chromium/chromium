// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/focus_mode_sounds_view.h"

#include <memory>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/error_message_toast.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_detailed_view.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/focus_mode/sounds/playlist_view.h"
#include "ash/system/focus_mode/sounds/sound_section_view.h"
#include "ash/system/model/system_tray_model.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Margins between containers in the detailed view if the container is not
// connected to the container above it.
constexpr auto kDisconnectedContainerMargins = gfx::Insets::TLBR(8, 0, 0, 0);

constexpr auto kSoundViewBottomPadding = 22;
constexpr auto kSoundTabSliderInsets = gfx::Insets::VH(16, 0);
constexpr auto kFocusSoundsLabelInsets = gfx::Insets::VH(18, 24);

constexpr int kNonPremiumChildViewsSpacing = 16;
constexpr int kNonPremiumLabelViewMaxWidth = 288;

constexpr float kOfflineStateOpacity = 0.38f;
constexpr auto kLabelPadding = gfx::Insets::VH(0, 40);

constexpr auto kErrorMessagePadding = gfx::Insets::TLBR(0, 4, 4, 4);
constexpr int kErrorMessageRoundedCornerRadius = 12;
constexpr gfx::Insets kErrorMessageButtonInsets =
    gfx::Insets::TLBR(8, 10, 8, 16);
constexpr gfx::Insets kErrorMessageLabelInsets = gfx::Insets::TLBR(8, 16, 8, 0);

// "01 Jan 2026 00:00 UTC" in milliseconds calculated by
// https://currentmillis.com/
constexpr base::Time kFreeTrialExpryTime =
    base::Time::FromMillisecondsSinceUnixEpoch(1767225600000L);

bool IsFreeTrialExpired(base::Time now) {
  return now >= kFreeTrialExpryTime;
}

std::unique_ptr<views::BoxLayoutView> CreateYouTubeMusicAlternateViewBase(
    const int label_message_id,
    const int button_message_id,
    views::Button::PressedCallback callback) {
  auto box_view = std::make_unique<views::BoxLayoutView>();
  box_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  box_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_view->SetBetweenChildSpacing(kNonPremiumChildViewsSpacing);

  auto* label = box_view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(label_message_id)));
  label->SetMultiLine(true);
  // For the label view with multiple lines, we need to set the max width for
  // it to calculate the total height of multiple lines.
  label->SetMaximumWidth(kNonPremiumLabelViewMaxWidth);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  label->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosBody2));
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

  auto* button = box_view->AddChildView(std::make_unique<PillButton>(
      std::move(callback), l10n_util::GetStringUTF16(button_message_id),
      PillButton::Type::kSecondaryWithoutIcon));
  button->SetBackgroundColorId(cros_tokens::kCrosSysHighlightShape);
  button->SetButtonTextColorId(cros_tokens::kCrosSysSystemOnPrimaryContainer);
  return box_view;
}

std::unique_ptr<views::BoxLayoutView> CreateFreeTrialView() {
  return CreateYouTubeMusicAlternateViewBase(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_FREE_TRIAL_LABEL,
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_FREE_TRIAL_BUTTON,
      base::BindRepeating([]() {
        FocusModeController::Get()
            ->focus_mode_sounds_controller()
            ->SavePrefForDisplayYouTubeMusicFreeTrial();

        Shell::Get()
            ->system_tray_model()
            ->client()
            ->ShowChromebookPerksYouTubePage();
      }));
}

std::unique_ptr<views::BoxLayoutView> CreateNonPremiumView() {
  return CreateYouTubeMusicAlternateViewBase(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_NON_PREMIUM_LABEL,
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_LEARN_MORE_BUTTON,
      base::BindRepeating([]() {
        Shell::Get()
            ->system_tray_model()
            ->client()
            ->ShowYouTubeMusicPremiumPage();
      }));
}

std::unique_ptr<views::BoxLayoutView> CreateOAuthView(
    views::Button::PressedCallback callback) {
  return CreateYouTubeMusicAlternateViewBase(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_OAUTH_CONSENT_LABEL,
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_OAUTH_CONSENT_BUTTON,
      std::move(callback));
}

std::unique_ptr<views::Label> CreateOfflineLabel(const int message_id) {
  auto label =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(message_id));
  label->SetFontList(ash::TypographyProvider::Get()->ResolveTypographyToken(
      ash::TypographyToken::kCrosBody2));
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  return label;
}

std::unique_ptr<views::BoxLayoutView> CreateOfflineStateView() {
  auto box_view = std::make_unique<views::BoxLayoutView>();
  box_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  box_view->SetBorder(views::CreateEmptyBorder(kLabelPadding));
  box_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_view->AddChildView(CreateOfflineLabel(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_OFFLINE_LABEL_ONE));
  box_view->AddChildView(CreateOfflineLabel(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_OFFLINE_LABEL_TWO));
  return box_view;
}

}  // namespace

//---------------------------------------------------------------------
// FocusModeSoundsView:

FocusModeSoundsView::FocusModeSoundsView(
    const base::flat_set<focus_mode_util::SoundType>& sound_sections,
    bool is_network_connected) {
  SetProperty(views::kMarginsKey, kDisconnectedContainerMargins);
  SetBorderInsets(gfx::Insets::TLBR(0, 0, kSoundViewBottomPadding, 0));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  if (sound_sections.empty()) {
    SetVisible(false);
    return;
  }
  CreateHeader(sound_sections, is_network_connected);

  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();

  const bool should_show_soundscapes =
      focus_mode_util::SoundType::kSoundscape ==
          sounds_controller->sound_type() ||
      !base::Contains(sound_sections,
                      focus_mode_util::SoundType::kYouTubeMusic);

  if (soundscape_button_ && youtube_music_button_) {
    if (should_show_soundscapes) {
      soundscape_button_->SetSelected(true);
    } else {
      youtube_music_button_->SetSelected(true);
    }
  }

  if (is_network_connected) {
    CreatesSoundSectionViews(sound_sections);
    if (should_show_soundscapes) {
      OnSoundscapeButtonToggled();
    } else {
      OnYouTubeMusicButtonToggled();
    }
  } else {
    AddChildView(CreateOfflineStateView());
  }

  sounds_controller->AddObserver(this);
}

FocusModeSoundsView::~FocusModeSoundsView() {
  FocusModeController::Get()->focus_mode_sounds_controller()->RemoveObserver(
      this);
}

void FocusModeSoundsView::Layout(PassKey) {
  LayoutSuperclass<RoundedContainer>(this);
  if (error_message_) {
    error_message_->UpdateBoundsToContainer(GetLocalBounds(),
                                            kErrorMessagePadding);
  }
}

void FocusModeSoundsView::OnSelectedPlaylistChanged() {
  const auto& selected_playlist = FocusModeController::Get()
                                      ->focus_mode_sounds_controller()
                                      ->selected_playlist();
  UpdateStateForSelectedPlaylist(selected_playlist);
}

void FocusModeSoundsView::OnPlaylistStateChanged() {
  const auto& selected_playlist = FocusModeController::Get()
                                      ->focus_mode_sounds_controller()
                                      ->selected_playlist();
  if (selected_playlist.empty()) {
    UpdateStateForSelectedPlaylist(selected_playlist);
    return;
  }

  switch (selected_playlist.type) {
    case focus_mode_util::SoundType::kSoundscape:
      if (soundscape_container_) {
        soundscape_container_->UpdateSelectedPlaylistForNewState(
            selected_playlist.state);
      }
      break;
    case focus_mode_util::SoundType::kYouTubeMusic:
      if (youtube_music_container_) {
        youtube_music_container_->UpdateSelectedPlaylistForNewState(
            selected_playlist.state);
      }
      break;
    case focus_mode_util::SoundType::kNone:
      NOTREACHED();
  }
}

void FocusModeSoundsView::OnPlayerError() {
  const auto& selected_playlist = FocusModeController::Get()
                                      ->focus_mode_sounds_controller()
                                      ->selected_playlist();
  if (selected_playlist.empty()) {
    LOG(WARNING) << "Media player error with no selected playlist";
    return;
  }

  ToastData data;
  data.source = selected_playlist.type;
  data.message =
      selected_playlist.type == focus_mode_util::SoundType::kSoundscape
          ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_FAILED_PLAYING_SOUNDS
          : IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_DISCONNECTED_WITH_YOUTUBE_MUSIC;
  data.action_type = ErrorMessageToast::ButtonActionType::kDismiss;
  data.fatal = false;
  ProcessError(data);
}

FocusModeSoundsView::ToastData::ToastData() = default;
FocusModeSoundsView::ToastData::ToastData(const ToastData&) = default;
FocusModeSoundsView::ToastData::~ToastData() = default;

constexpr std::partial_ordering FocusModeSoundsView::ToastData::operator<=>(
    const ToastData& other) const {
  if (source != other.source) {
    return std::partial_ordering::unordered;
  }

  if (fatal) {
    if (!other.fatal) {
      return std::partial_ordering::greater;
    }
  } else {
    if (other.fatal) {
      return std::partial_ordering::less;
    }
  }

  if (absl::holds_alternative<int>(message)) {
    if (absl::holds_alternative<std::u16string>(other.message)) {
      return std::partial_ordering::less;
    }
  } else {
    if (absl::holds_alternative<int>(other.message)) {
      return std::partial_ordering::greater;
    }
  }

  switch (action_type) {
    case ErrorMessageToast::ButtonActionType::kDismiss:
      if (other.action_type == ErrorMessageToast::ButtonActionType::kReload) {
        return std::partial_ordering::greater;
      }
      break;
    case ErrorMessageToast::ButtonActionType::kReload:
      if (other.action_type == ErrorMessageToast::ButtonActionType::kDismiss) {
        return std::partial_ordering::less;
      }
      break;
  }

  if (message != other.message) {
    // `ToastData` that is otherwise equivalent but only differs in the content
    // of `message` are not ordered but not equal.
    return std::partial_ordering::unordered;
  }

  return std::partial_ordering::equivalent;
}

void FocusModeSoundsView::UpdateSoundsView(
    bool is_soundscape_type,
    const std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>&
        playlists) {
  if (is_soundscape_type) {
    if (!soundscape_container_) {
      return;
    }
    if (playlists.size() == kFocusModePlaylistViewsNum) {
      soundscape_container_->UpdateContents(playlists);
      return;
    }

    ToastData data;
    data.source = focus_mode_util::SoundType::kSoundscape;
    data.message =
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_DISCONNECTED_WITH_FOCUS_SOUNDS;
    data.action_type = ErrorMessageToast::ButtonActionType::kReload;
    data.fatal = false;
    ProcessError(data);
    return;
  }

  if (!youtube_music_container_) {
    return;
  }

  if (playlists.size() == kFocusModePlaylistViewsNum) {
    youtube_music_container_->UpdateContents(playlists);
    return;
  }

  if (!youtube_music_container_->IsAlternateViewVisible()) {
    ToastData data;
    data.source = focus_mode_util::SoundType::kYouTubeMusic;
    data.message =
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_DISCONNECTED_WITH_YOUTUBE_MUSIC;
    data.action_type = ErrorMessageToast::ButtonActionType::kReload;
    data.fatal = false;
    ProcessError(data);
    return;
  }
}

void FocusModeSoundsView::UpdateStateForSelectedPlaylist(
    const focus_mode_util::SelectedPlaylist& selected_playlist) {
  if (soundscape_container_) {
    soundscape_container_->UpdateStateForSelectedPlaylist(selected_playlist);
  }
  if (youtube_music_container_) {
    youtube_music_container_->UpdateStateForSelectedPlaylist(selected_playlist);
  }
}

void FocusModeSoundsView::CreateHeader(
    const base::flat_set<focus_mode_util::SoundType>& sections,
    bool is_network_connected) {
  CHECK(!sections.empty());
  CHECK(base::Contains(sections, focus_mode_util::SoundType::kSoundscape));
  const bool contains_youtube_music =
      base::Contains(sections, focus_mode_util::SoundType::kYouTubeMusic);

  auto* sounds_container_header =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  sounds_container_header->SetInsideBorderInsets(
      contains_youtube_music ? kSoundTabSliderInsets : kFocusSoundsLabelInsets);
  sounds_container_header->SetMainAxisAlignment(
      contains_youtube_music ? views::BoxLayout::MainAxisAlignment::kCenter
                             : views::BoxLayout::MainAxisAlignment::kStart);

  // If there is no YouTube Music type of playlists, we can just create a label.
  if (!contains_youtube_music) {
    auto* focus_sounds_label =
        sounds_container_header->AddChildView(std::make_unique<views::Label>());
    focus_sounds_label->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_SOUNDSCAPE_BUTTON));
    focus_sounds_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    focus_sounds_label->SetEnabledColorId(
        cros_tokens::kCrosSysOnSurfaceVariant);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                          *focus_sounds_label);
    return;
  }

  auto* sound_tab_slider = sounds_container_header->AddChildView(
      std::make_unique<TabSlider>(/*max_tab_num=*/2));
  sound_tab_slider->GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);

  soundscape_button_ = sound_tab_slider->AddButton<IconLabelSliderButton>(
      base::BindRepeating(&FocusModeSoundsView::OnSoundscapeButtonToggled,
                          weak_factory_.GetWeakPtr()),
      &kFocusSoundsIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_SOUNDSCAPE_BUTTON),
      /*tooltip_text_base=*/u"", /*horizontal=*/true);
  soundscape_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kTab);

  youtube_music_button_ = sound_tab_slider->AddButton<IconLabelSliderButton>(
      base::BindRepeating(&FocusModeSoundsView::OnYouTubeMusicButtonToggled,
                          weak_factory_.GetWeakPtr()),
      &kYtmIcon,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_YOUTUBE_MUSIC_BUTTON),
      /*tooltip_text_base=*/u"", /*horizontal=*/true);
  youtube_music_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kTab);

  if (!is_network_connected) {
    sound_tab_slider->layer()->SetOpacity(kOfflineStateOpacity);
    sound_tab_slider->SetEnabled(false);
  }
}

void FocusModeSoundsView::CreatesSoundSectionViews(
    const base::flat_set<focus_mode_util::SoundType>& sound_sections) {
  if (base::Contains(sound_sections, focus_mode_util::SoundType::kSoundscape)) {
    soundscape_container_ = AddChildView(std::make_unique<SoundSectionView>(
        focus_mode_util::SoundType::kSoundscape));
    // Start downloading playlists for Soundscape.
    DownloadPlaylistsForType(/*is_soundscape_type=*/true);
  }

  if (base::Contains(sound_sections,
                     focus_mode_util::SoundType::kYouTubeMusic)) {
    youtube_music_container_ = AddChildView(std::make_unique<SoundSectionView>(
        focus_mode_util::SoundType::kYouTubeMusic));

    auto* sounds_controller =
        FocusModeController::Get()->focus_mode_sounds_controller();
    if (sounds_controller->ShouldDisplayYouTubeMusicOAuth()) {
      youtube_music_container_->SetAlternateView(
          CreateOAuthView(base::BindRepeating(
              &FocusModeSoundsView::OnOAuthGetStartedButtonPressed,
              weak_factory_.GetWeakPtr())));
      ToggleYouTubeMusicAlternateView(/*show=*/true);
      return;
    }

    if (!sounds_controller->IsMinorUser() &&
        sounds_controller->ShouldDisplayYouTubeMusicFreeTrial() &&
        !IsFreeTrialExpired(base::Time::Now())) {
      youtube_music_container_->SetAlternateView(CreateFreeTrialView());
    } else {
      youtube_music_container_->SetAlternateView(CreateNonPremiumView());
    }
    ToggleYouTubeMusicAlternateView(/*show=*/false);
    DownloadPlaylistsForType(/*is_soundscape_type=*/false);
  }
}

void FocusModeSoundsView::OnOAuthGetStartedButtonPressed() {
  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();
  sounds_controller->SavePrefForDisplayYouTubeMusicOAuth();

  // Sets the alternate view to either the free trial view or the non-premium
  // view for `youtube_music_container_`.
  if (!sounds_controller->IsMinorUser() &&
      sounds_controller->ShouldDisplayYouTubeMusicFreeTrial() &&
      !IsFreeTrialExpired(base::Time::Now())) {
    youtube_music_container_->SetAlternateView(CreateFreeTrialView());
  } else {
    youtube_music_container_->SetAlternateView(CreateNonPremiumView());
  }
  ToggleYouTubeMusicAlternateView(/*show=*/false);
  DownloadPlaylistsForType(/*is_soundscape_type=*/false);
}

void FocusModeSoundsView::ToggleYouTubeMusicAlternateView(bool show) {
  CHECK(youtube_music_container_);
  youtube_music_container_->ShowAlternateView(show);
  MaybeDismissErrorMessage();
}

void FocusModeSoundsView::YouTubeMusicError(
    const FocusModeApiError& api_error) {
  const std::string& error_message = api_error.error_message;

  ToastData data;
  data.source = focus_mode_util::SoundType::kYouTubeMusic;
  if (error_message.empty()) {
    data.message =
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_SOUNDS_DISCONNECTED_WITH_YOUTUBE_MUSIC;
  } else {
    data.message = base::UTF8ToUTF16(error_message);
  }
  data.action_type = ErrorMessageToast::ButtonActionType::kDismiss;
  data.fatal = api_error.fatal;
  ProcessError(data);
}

void FocusModeSoundsView::OnSoundscapeButtonToggled() {
  MayShowSoundscapeContainer(true);
}

void FocusModeSoundsView::OnYouTubeMusicButtonToggled() {
  MayShowSoundscapeContainer(false);
  const std::optional<FocusModeApiError>& api_error =
      FocusModeController::Get()
          ->focus_mode_sounds_controller()
          ->last_youtube_music_error();
  if (api_error) {
    // Show the error if it's still available.
    YouTubeMusicError(*api_error);
  }
}

void FocusModeSoundsView::MayShowSoundscapeContainer(bool show) {
  MaybeDismissErrorMessage();
  if (soundscape_container_) {
    soundscape_container_->SetVisible(show);
  }
  if (youtube_music_container_) {
    youtube_music_container_->SetVisible(!show);
  }

  if (soundscape_button_) {
    soundscape_button_->GetViewAccessibility().SetIsSelected(show);
    youtube_music_button_->GetViewAccessibility().SetIsSelected(!show);
  }
}

void FocusModeSoundsView::DownloadPlaylistsForType(bool is_soundscape_type) {
  auto* sounds_controller =
      FocusModeController::Get()->focus_mode_sounds_controller();

  // Set the no premium callback and error callback for YTM only if we start
  // downloading the YTM playlists.
  if (!is_soundscape_type) {
    sounds_controller->SetYouTubeMusicNoPremiumCallback(base::BindRepeating(
        &FocusModeSoundsView::ToggleYouTubeMusicAlternateView,
        weak_factory_.GetWeakPtr(), /*show=*/true));
    sounds_controller->SetErrorCallback(
        is_soundscape_type,
        base::BindRepeating(&FocusModeSoundsView::YouTubeMusicError,
                            weak_factory_.GetWeakPtr()));
  }

  sounds_controller->DownloadPlaylistsForType(
      is_soundscape_type, base::BindOnce(&FocusModeSoundsView::UpdateSoundsView,
                                         weak_factory_.GetWeakPtr()));
}

void FocusModeSoundsView::MaybeDismissErrorMessage() {
  if (!error_message_.get()) {
    return;
  }

  RemoveChildViewT(std::exchange(error_message_, nullptr));
}

void FocusModeSoundsView::ProcessError(const ToastData& data) {
  focus_mode_util::SoundType backend = data.source;
  if (backend == focus_mode_util::SoundType::kNone) {
    return;
  }

  // Check if this error is more severe than the previous error.
  // Only YTM handles persistent errors currently.
  if (backend == focus_mode_util::SoundType::kYouTubeMusic) {
    if (youtube_music_api_error_.has_value() &&
        data < *youtube_music_api_error_) {
      // If the previous error is more severe, skip this error.
      return;
    }

    // This is the first error or a more severe error.
    youtube_music_api_error_ = data;
  }

  const std::u16string& message =
      absl::holds_alternative<std::u16string>(data.message)
          ? absl::get<std::u16string>(data.message)
          : l10n_util::GetStringUTF16(absl::get<int>(data.message));
  ShowErrorMessageForType(
      data.source == focus_mode_util::SoundType::kSoundscape, message,
      data.action_type);
}

void FocusModeSoundsView::ShowErrorMessageForType(
    bool is_soundscape_type,
    const std::u16string& message,
    ErrorMessageToast::ButtonActionType type) {
  MaybeDismissErrorMessage();

  views::Button::PressedCallback callback;
  switch (type) {
    case ErrorMessageToast::ButtonActionType::kDismiss:
      callback =
          base::BindRepeating(&FocusModeSoundsView::MaybeDismissErrorMessage,
                              weak_factory_.GetWeakPtr());
      break;
    case ErrorMessageToast::ButtonActionType::kReload:
      callback =
          base::BindRepeating(&FocusModeSoundsView::DownloadPlaylistsForType,
                              weak_factory_.GetWeakPtr(), is_soundscape_type);
      break;
  }

  error_message_ = AddChildView(std::make_unique<ErrorMessageToast>(
      std::move(callback), message, type, cros_tokens::kCrosSysSystemBase));
  error_message_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  error_message_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kErrorMessageRoundedCornerRadius));
  error_message_->error_message_label()->SetProperty(views::kMarginsKey,
                                                     kErrorMessageLabelInsets);
  error_message_->action_button()->SetProperty(views::kMarginsKey,
                                               kErrorMessageButtonInsets);
}

BEGIN_METADATA(FocusModeSoundsView)
END_METADATA

}  // namespace ash
