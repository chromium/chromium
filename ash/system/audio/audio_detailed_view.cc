// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_detailed_view.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/typography.h"
#include "ash/system/audio/audio_detailed_view_utils.h"
#include "ash/system/audio/labeled_slider_view.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/audio/mic_gain_slider_view.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/audio/unified_volume_view.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/quick_settings_slider.h"
#include "ash/system/unified/unified_slider_view.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

const int kToggleButtonRowRightPadding = 16;
const int kNbsWarningMinHeight = 80;
constexpr auto kLiveCaptionContainerMargins = gfx::Insets::TLBR(0, 0, 8, 0);
constexpr auto kToggleButtonRowViewPadding =
    gfx::Insets::TLBR(0, 32, 0, kToggleButtonRowRightPadding);
constexpr auto kToggleButtonRowPreferredSize = gfx::Size(0, 32);
constexpr auto kToggleButtonRowLabelPadding = gfx::Insets::VH(8, 12);
constexpr auto kToggleButtonRowMargins = gfx::Insets::VH(4, 0);
constexpr auto kSeparatorMargins = gfx::Insets::TLBR(4, 32, 12, 32);
constexpr auto kTextRowInsets = gfx::Insets::VH(8, 24);

// This callback is only used for tests.
AudioDetailedView::NoiseCancellationCallback*
    g_noise_cancellation_toggle_callback = nullptr;

// This callback is only used for tests.
AudioDetailedView::StyleTransferCallback* g_style_transfer_toggle_callback =
    nullptr;

speech::LanguageCode GetLiveCaptionLocale() {
  std::string live_caption_locale = speech::kUsEnglishLocale;
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (pref_service) {
    live_caption_locale = ::prefs::GetLiveCaptionLanguageCode(pref_service);
  }
  return speech::GetLanguageCode(live_caption_locale);
}

std::vector<std::string> GetNamesOfAppsAccessingMic(
    apps::AppRegistryCache* app_registry_cache,
    apps::AppCapabilityAccessCache* app_capability_access_cache) {
  if (!app_registry_cache || !app_capability_access_cache) {
    return {};
  }

  std::vector<std::string> app_names;
  for (const std::string& app :
       app_capability_access_cache->GetAppsAccessingMicrophone()) {
    std::string name;
    app_registry_cache->ForOneApp(
        app, [&name](const apps::AppUpdate& update) { name = update.Name(); });
    if (!name.empty()) {
      app_names.push_back(name);
    }
  }

  return app_names;
}

std::u16string GetTextForAgcInfo(const std::vector<std::string>& app_names) {
  std::u16string agc_info_string = l10n_util::GetPluralStringFUTF16(
      IDS_ASH_STATUS_TRAY_AUDIO_INPUT_AGC_INFO, app_names.size());
  return app_names.size() == 1
             ? l10n_util::FormatString(
                   agc_info_string, {base::UTF8ToUTF16(app_names[0])}, nullptr)
             : agc_info_string;
}

void AddSeparator(views::View* container) {
  auto* separator =
      container->AddChildView(std::make_unique<views::Separator>());
  separator->SetColorId(cros_tokens::kCrosSysSeparator);
  separator->SetOrientation(views::Separator::Orientation::kHorizontal);
  separator->SetProperty(views::kMarginsKey, kSeparatorMargins);
}

}  // namespace

AudioDetailedView::AudioDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate),
      num_stream_ignore_ui_gains_(
          CrasAudioHandler::Get()->num_stream_ignore_ui_gains()) {
  CreateItems();

  Shell::Get()->accessibility_controller()->AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);

  if (captions::IsLiveCaptionFeatureSupported()) {
    speech::SodaInstaller* soda_installer =
        speech::SodaInstaller::GetInstance();
    if (soda_installer) {
      soda_installer->AddObserver(this);
    }
  }

  // Session state observer currently only used for monitoring the microphone
  // usage which is only for the information for showing AGC control.
  if (base::FeatureList::IsEnabled(media::kIgnoreUiGains)) {
    session_observation_.Observe(Shell::Get()->session_controller());

    // Initialize with current session state.
    OnSessionStateChanged(
        Shell::Get()->session_controller()->GetSessionState());
  }
}

AudioDetailedView::~AudioDetailedView() {
  if (captions::IsLiveCaptionFeatureSupported()) {
    speech::SodaInstaller* soda_installer =
        speech::SodaInstaller::GetInstance();
    // `soda_installer` is not guaranteed to be valid, since it's possible for
    // this class to out-live it. This means that this class cannot use
    // ScopedObservation and needs to manage removing the observer itself.
    if (soda_installer) {
      soda_installer->RemoveObserver(this);
    }
  }
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

views::View* AudioDetailedView::GetAsView() {
  return this;
}

void AudioDetailedView::SetMapNoiseCancellationToggleCallbackForTest(
    AudioDetailedView::NoiseCancellationCallback*
        noise_cancellation_toggle_callback) {
  g_noise_cancellation_toggle_callback = noise_cancellation_toggle_callback;
}

void AudioDetailedView::SetMapStyleTransferToggleCallbackForTest(
    AudioDetailedView::StyleTransferCallback* style_transfer_toggle_callback) {
  g_style_transfer_toggle_callback = style_transfer_toggle_callback;
}

void AudioDetailedView::Update() {
  UpdateAudioDevices();
  DeprecatedLayoutImmediately();
}

void AudioDetailedView::OnAccessibilityStatusChanged() {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  // The live caption state has been updated.
  UpdateLiveCaptionView(controller->live_caption().enabled());
}

void AudioDetailedView::OnCapabilityAccessUpdate(
    const apps::CapabilityAccessUpdate& update) {
  UpdateAgcInfoRow();
}

void AudioDetailedView::OnAppCapabilityAccessCacheWillBeDestroyed(
    apps::AppCapabilityAccessCache* cache) {
  app_capability_observation_.Reset();
  app_capability_access_cache_ = nullptr;
}

void AudioDetailedView::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Session state observer currently only used for monitoring the microphone
  // usage which is only for the information for showing AGC control.
  if (!base::FeatureList::IsEnabled(media::kIgnoreUiGains)) {
    return;
  }
  app_capability_observation_.Reset();
  app_registry_cache_ = nullptr;
  app_capability_access_cache_ = nullptr;
  if (state != session_manager::SessionState::ACTIVE) {
    return;
  }
  auto* session_controller = Shell::Get()->session_controller();
  if (!session_controller) {
    return;
  }

  AccountId active_user_account_id = session_controller->GetActiveAccountId();
  if (!active_user_account_id.is_valid()) {
    return;
  }

  app_registry_cache_ =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
          active_user_account_id);
  app_capability_access_cache_ =
      apps::AppCapabilityAccessCacheWrapper::Get().GetAppCapabilityAccessCache(
          active_user_account_id);

  if (app_capability_access_cache_) {
    app_capability_observation_.Observe(app_capability_access_cache_);
  }
}

void AudioDetailedView::AddAudioSubHeader(views::View* container,
                                          const gfx::VectorIcon& icon,
                                          const int text_id) {
  auto* sub_header_label_ = TrayPopupUtils::CreateDefaultLabel();
  sub_header_label_->SetText(l10n_util::GetStringUTF16(text_id));
  sub_header_label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  sub_header_label_->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                        *sub_header_label_);
  sub_header_label_->SetBorder(views::CreateEmptyBorder(kTextRowInsets));
  container->AddChildView(sub_header_label_);
  return;
}

void AudioDetailedView::CreateItems() {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_AUDIO_TITLE);

  if (captions::IsLiveCaptionFeatureSupported()) {
    CreateLiveCaptionView();
  }

  mic_gain_controller_ = std::make_unique<MicGainSliderController>();
  unified_volume_slider_controller_ =
      std::make_unique<UnifiedVolumeSliderController>();
}

void AudioDetailedView::CreateLiveCaptionView() {
  auto* live_caption_container =
      scroll_content()->AddChildViewAt(std::make_unique<RoundedContainer>(), 0);
  live_caption_container->SetProperty(views::kMarginsKey,
                                      kLiveCaptionContainerMargins);
  // Ensures the `HoverHighlightView` ink drop fills the whole container.
  live_caption_container->SetBorderInsets(gfx::Insets());

  live_caption_view_ = live_caption_container->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  live_caption_view_->SetFocusBehavior(FocusBehavior::NEVER);

  // Creates the icon and text for the `live_caption_view_`.
  const bool live_caption_enabled =
      Shell::Get()->accessibility_controller()->live_caption().enabled();
  auto toggle_icon = std::make_unique<views::ImageView>();
  toggle_icon->SetImage(ui::ImageModel::FromVectorIcon(
      live_caption_enabled ? kUnifiedMenuLiveCaptionIcon
                           : kUnifiedMenuLiveCaptionOffIcon,
      cros_tokens::kCrosSysOnSurface, kQsSliderIconSize));
  live_caption_icon_ = toggle_icon.get();
  live_caption_view_->AddViewAndLabel(
      std::move(toggle_icon),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION));
  live_caption_view_->text_label()->SetEnabledColorId(
      cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                        *live_caption_view_->text_label());

  // Creates a toggle button on the right.
  auto toggle = std::make_unique<Switch>(base::BindRepeating(
      &AudioDetailedView::ToggleLiveCaptionState, weak_factory_.GetWeakPtr()));
  toggle->SetIsOn(live_caption_enabled);
  std::u16string toggle_tooltip =
      live_caption_enabled
          ? l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_LIVE_CAPTION_ENABLED_STATE_TOOLTIP)
          : l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP);
  toggle->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP, toggle_tooltip));
  toggle->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION));
  live_caption_button_ = toggle.get();
  live_caption_view_->AddRightView(toggle.release());

  // Allows the row to be taller than a typical tray menu item.
  live_caption_view_->SetExpandable(true);
  live_caption_view_->tri_view()->SetInsets(
      gfx::Insets::TLBR(8, 24, 8, kToggleButtonRowRightPadding));
}

std::unique_ptr<TriView> AudioDetailedView::CreateNbsWarningView() {
  std::unique_ptr<TriView> nbs_warning_view(
      TrayPopupUtils::CreateDefaultRowView(
          /*use_wide_layout=*/true));
  nbs_warning_view->SetMinHeight(kNbsWarningMinHeight);
  nbs_warning_view->SetContainerVisible(TriView::Container::END, false);
  nbs_warning_view->SetID(AudioDetailedViewID::kNbsWarningView);

  std::unique_ptr<views::ImageView> image_view =
      base::WrapUnique(TrayPopupUtils::CreateMainImageView(
          /*use_wide_layout=*/true));
  image_view->SetImage(
      ui::ImageModel::FromVectorIcon(vector_icons::kNotificationWarningIcon,
                                     kColorAshIconColorWarning, kMenuIconSize));
  nbs_warning_view->AddView(TriView::Container::START, std::move(image_view));

  std::unique_ptr<views::Label> label =
      base::WrapUnique(TrayPopupUtils::CreateDefaultLabel());
  label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_NBS_MESSAGE));
  label->SetMultiLine(/*multi_line=*/true);
  label->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  label->SetEnabledColorId(kColorAshTextColorWarning);
  label->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *label);

  nbs_warning_view->AddView(TriView::Container::CENTER, std::move(label));
  return nbs_warning_view;
}

std::unique_ptr<HoverHighlightView>
AudioDetailedView::CreateNoiseCancellationToggleRow(const AudioDevice& device) {
  bool noise_cancellation_state =
      CrasAudioHandler::Get()->GetNoiseCancellationState();

  auto noise_cancellation_view =
      std::make_unique<HoverHighlightView>(/*listener=*/this);

  auto toggle_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kUnifiedMenuMicNoiseCancelHighIcon, cros_tokens::kCrosSysOnSurface,
          kQsSliderIconSize));
  noise_cancellation_icon_ = toggle_icon.get();

  noise_cancellation_view->AddViewAndLabel(
      std::move(toggle_icon),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION));
  views::Label* noise_cancellation_label =
      noise_cancellation_view->text_label();
  noise_cancellation_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                        *noise_cancellation_label);

  // Create a non-clickable non-focusable toggle button on the right. The events
  // and focus behavior should be handled by `noise_cancellation_view_` instead.
  auto toggle = std::make_unique<Switch>();
  toggle->SetIsOn(noise_cancellation_state);
  toggle->SetCanProcessEventsWithinSubtree(false);
  toggle->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  // Ignore the toggle for accessibility.
  auto& view_accessibility = toggle->GetViewAccessibility();
  view_accessibility.SetIsLeaf(true);
  view_accessibility.SetIsIgnored(true);
  noise_cancellation_button_ = toggle.get();
  noise_cancellation_view->AddRightView(toggle.release());

  noise_cancellation_view->tri_view()->SetInsets(kToggleButtonRowViewPadding);
  noise_cancellation_view->tri_view()->SetContainerLayout(
      TriView::Container::CENTER, std::make_unique<views::BoxLayout>(
                                      views::BoxLayout::Orientation::kVertical,
                                      kToggleButtonRowLabelPadding));
  noise_cancellation_view->SetPreferredSize(kToggleButtonRowPreferredSize);
  noise_cancellation_view->SetProperty(views::kMarginsKey,
                                       kToggleButtonRowMargins);
  noise_cancellation_view->SetAccessibilityState(
      noise_cancellation_state
          ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
          : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);

  // This is only used for testing.
  if (g_noise_cancellation_toggle_callback) {
    g_noise_cancellation_toggle_callback->Run(device.id,
                                              noise_cancellation_view.get());
  }

  return noise_cancellation_view;
}

std::unique_ptr<HoverHighlightView>
AudioDetailedView::CreateStyleTransferToggleRow(const AudioDevice& device) {
  auto toggle_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kUnifiedMenuMicStyleTransferIcon, cros_tokens::kCrosSysOnSurface,
          kQsSliderIconSize));
  style_transfer_icon_ = toggle_icon.get();

  auto style_transfer_view =
      std::make_unique<HoverHighlightView>(/*listener=*/this);
  style_transfer_view->AddViewAndLabel(
      std::move(toggle_icon),
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INPUT_STYLE_TRANSFER));
  views::Label* style_transfer_label = style_transfer_view->text_label();
  style_transfer_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                        *style_transfer_label);

  // Create a non-clickable non-focusable toggle button on the right. The events
  // and focus behavior should be handled by `style_transfer_view_` instead.
  const bool style_transfer_state =
      CrasAudioHandler::Get()->GetStyleTransferState();

  auto toggle = std::make_unique<Switch>();
  toggle->SetIsOn(style_transfer_state);
  toggle->SetCanProcessEventsWithinSubtree(false);
  toggle->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  // Ignore the toggle for accessibility.
  auto& view_accessibility = toggle->GetViewAccessibility();
  view_accessibility.SetIsLeaf(true);
  view_accessibility.SetIsIgnored(true);
  style_transfer_button_ = toggle.get();
  style_transfer_view->AddRightView(toggle.release());

  style_transfer_view->tri_view()->SetInsets(kToggleButtonRowViewPadding);
  style_transfer_view->tri_view()->SetContainerLayout(
      TriView::Container::CENTER, std::make_unique<views::BoxLayout>(
                                      views::BoxLayout::Orientation::kVertical,
                                      kToggleButtonRowLabelPadding));
  style_transfer_view->SetPreferredSize(kToggleButtonRowPreferredSize);
  style_transfer_view->SetProperty(views::kMarginsKey, kToggleButtonRowMargins);
  style_transfer_view->SetAccessibilityState(
      style_transfer_state
          ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
          : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);

  // This is only used for testing.
  if (g_style_transfer_toggle_callback) {
    g_style_transfer_toggle_callback->Run(device.id, style_transfer_view.get());
  }

  return style_transfer_view;
}

std::unique_ptr<HoverHighlightView> AudioDetailedView::CreateAgcInfoRow(
    const AudioDevice& device) {
  auto agc_info_view = std::make_unique<HoverHighlightView>(/*listener=*/this);
  agc_info_view->SetID(AudioDetailedViewID::kAgcInfoView);

  auto info_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          kUnifiedMenuInfoIcon, cros_tokens::kCrosSysOnSurface,
          kQsSliderIconSize));
  agc_info_view->AddViewAndLabel(
      std::move(info_icon),
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INPUT_AGC_INFO,
                                 std::u16string()));
  views::Label* text_label = agc_info_view->text_label();
  CHECK(text_label);
  text_label->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  if (base::FeatureList::IsEnabled(media::kShowForceRespectUiGainsToggle)) {
    // Add settings button to link to the audio settings page.
    auto settings = std::make_unique<PillButton>(
        base::BindRepeating(&AudioDetailedView::OnSettingsButtonClicked,
                            weak_factory_.GetWeakPtr()),
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_AUDIO_SETTINGS_SHORT_STRING),
        PillButton::Type::kFloatingWithoutIcon,
        /*icon=*/nullptr);
    if (!TrayPopupUtils::CanOpenWebUISettings()) {
      settings->SetEnabled(false);
    }
    agc_info_view->AddRightView(settings.release());
  }

  agc_info_view->tri_view()->SetInsets(kToggleButtonRowViewPadding);
  agc_info_view->tri_view()->SetContainerLayout(
      TriView::Container::CENTER, std::make_unique<views::BoxLayout>(
                                      views::BoxLayout::Orientation::kVertical,
                                      kToggleButtonRowLabelPadding));
  agc_info_view->SetPreferredSize(kToggleButtonRowPreferredSize);
  agc_info_view->SetProperty(views::kMarginsKey, kToggleButtonRowMargins);
  agc_info_view->SetFocusBehavior(FocusBehavior::NEVER);

  return agc_info_view;
}

LabeledSliderView* AudioDetailedView::CreateLabeledSliderView(
    views::View* container,
    const AudioDevice& device) {
  std::unique_ptr<views::View> slider;
  if (device.is_input) {
    slider = mic_gain_controller_->CreateMicGainSlider(device.id,
                                                       device.IsInternalMic());
  } else {
    slider = unified_volume_slider_controller_->CreateVolumeSlider(device.id);
    if (device.active) {
      views::AsViewClass<QuickSettingsSlider>(
          views::AsViewClass<UnifiedVolumeView>(slider.get())->slider())
          ->set_is_toggleable_volume_slider(true);
    }
  }

  auto* labeled_slider_view = views::AsViewClass<LabeledSliderView>(
      container->AddChildView(std::make_unique<LabeledSliderView>(
          /*detailed_view=*/this, std::move(slider), device,
          /*is_wide_slider=*/false)));
  device_map_[labeled_slider_view->device_name_view()] = device;

  // If the `device_name_container` of this device is previously focused and
  // then becomes active, the slider of this device should preserve the focus.
  if (focused_device_id_ == device.id && device.active) {
    labeled_slider_view->unified_slider_view()->slider()->RequestFocus();
    // Resets the id.
    focused_device_id_ = std::nullopt;
  }

  return labeled_slider_view;
}

void AudioDetailedView::MaybeShowSodaMessage(speech::LanguageCode language_code,
                                             std::u16string message) {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  const bool is_live_caption_enabled = controller->live_caption().enabled();
  // Only show updates for this feature if the language code applies to the SODA
  // binary (encoded by `LanguageCode::kNone`) or the language pack matching
  // the feature locale.
  const bool live_caption_has_update =
      language_code == speech::LanguageCode::kNone ||
      language_code == GetLiveCaptionLocale();

  if (live_caption_has_update && is_live_caption_enabled) {
    live_caption_view_->SetSubText(message);
  }
}

void AudioDetailedView::OnInputNoiseCancellationTogglePressed() {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  const bool new_state = !audio_handler->GetNoiseCancellationState();
  audio_handler->SetNoiseCancellationState(
      new_state, CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);
  noise_cancellation_button_->SetIsOn(new_state);
}

void AudioDetailedView::OnInputStyleTransferTogglePressed() {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  const bool new_state = !audio_handler->GetStyleTransferState();
  audio_handler->SetStyleTransferState(new_state);
  style_transfer_button_->SetIsOn(new_state);
  style_transfer_view_->RequestFocus();
}

void AudioDetailedView::OnSettingsButtonClicked() {
  if (!TrayPopupUtils::CanOpenWebUISettings()) {
    return;
  }

  CloseBubble();  // Deletes |this|.
  Shell::Get()->system_tray_model()->client()->ShowAudioSettings();
}

void AudioDetailedView::ToggleLiveCaptionState() {
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  // Updates the enable state for live caption.
  controller->live_caption().SetEnabled(!controller->live_caption().enabled());
}

void AudioDetailedView::UpdateLiveCaptionView(bool is_enabled) {
  live_caption_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      is_enabled ? kUnifiedMenuLiveCaptionIcon : kUnifiedMenuLiveCaptionOffIcon,
      cros_tokens::kCrosSysOnSurface, kQsSliderIconSize));

  // Updates the toggle button tooltip.
  std::u16string toggle_tooltip =
      is_enabled ? l10n_util::GetStringUTF16(
                       IDS_ASH_STATUS_TRAY_LIVE_CAPTION_ENABLED_STATE_TOOLTIP)
                 : l10n_util::GetStringUTF16(
                       IDS_ASH_STATUS_TRAY_LIVE_CAPTION_DISABLED_STATE_TOOLTIP);
  live_caption_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_LIVE_CAPTION_TOGGLE_TOOLTIP, toggle_tooltip));

  // Ensures the toggle button is in sync with the current Live Caption state.
  if (live_caption_button_->GetIsOn() != is_enabled) {
    live_caption_button_->SetIsOn(is_enabled);
  }

  InvalidateLayout();
}

void AudioDetailedView::UpdateAudioDevices() {
  output_devices_.clear();
  input_devices_.clear();
  AudioDeviceList devices;
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  audio_handler->GetAudioDevices(&devices);
  const bool has_dual_internal_mic = audio_handler->HasDualInternalMic();
  bool is_front_or_rear_mic_active = false;
  for (const auto& device : devices) {
    // Only display devices if they are for simple usage.
    if (!device.is_for_simple_usage()) {
      continue;
    }
    if (device.is_input) {
      // Do not expose the internal front and rear mic to UI.
      if (has_dual_internal_mic && audio_handler->IsFrontOrRearMic(device)) {
        if (device.active) {
          is_front_or_rear_mic_active = true;
        }
        continue;
      }
      input_devices_.push_back(device);
    } else {
      output_devices_.push_back(device);
    }
  }

  // Expose the dual internal mics as one device (internal mic) to user.
  if (has_dual_internal_mic) {
    // Create stub internal mic entry for UI rendering, which representing
    // both internal front and rear mics.
    AudioDevice internal_mic;
    internal_mic.is_input = true;
    // `stable_device_id_version` is used to differentiate `stable_device_id`
    // for backward compatibility. Version 2 means `deprecated_stable_device_id`
    // will contain deprecated, v1 stable device id version.
    internal_mic.stable_device_id_version = 2;
    internal_mic.type = AudioDeviceType::kInternalMic;
    internal_mic.active = is_front_or_rear_mic_active;
    input_devices_.push_back(internal_mic);
  }

  UpdateScrollableList();
}

void AudioDetailedView::AddSeparatorIfNotLast(views::View* container,
                                             const AudioDevice& device) {
  if (device.is_input ? &device != &input_devices_.back()
                      : &device != &output_devices_.back()) {
    AddSeparator(container);
  }
}

void AudioDetailedView::UpdateScrollableList() {
  // Resets all raw pointers inside the `scroll_content()`. Otherwise it can
  // lead to a crash when the the view is clicked. Also clears `device_map_`
  // before removing all child views since it holds pointers to child views in
  // `scroll_content()`.
  noise_cancellation_view_ = nullptr;
  noise_cancellation_icon_ = nullptr;
  noise_cancellation_button_ = nullptr;
  style_transfer_view_ = nullptr;
  style_transfer_icon_ = nullptr;
  style_transfer_button_ = nullptr;
  live_caption_view_ = nullptr;
  live_caption_icon_ = nullptr;
  live_caption_button_ = nullptr;
  device_map_.clear();
  scroll_content()->RemoveAllChildViews();

  views::View* container =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>());

  if (captions::IsLiveCaptionFeatureSupported()) {
    // Adds the live caption toggle.
    CreateLiveCaptionView();
  }

  // Adds audio output devices.
  const bool has_output_devices = output_devices_.size() > 0;
  if (has_output_devices) {
    AddAudioSubHeader(container, kSystemMenuAudioOutputIcon,
                      IDS_ASH_STATUS_TRAY_AUDIO_OUTPUT);
  }

  LabeledSliderView* last_output_device = nullptr;
  for (const auto& device : output_devices_) {
    last_output_device = CreateLabeledSliderView(container, device);
  }

  if (has_output_devices) {
    last_output_device->SetProperty(views::kMarginsKey, kSubsectionMargins);
  }

  // Adds audio input devices.
  const bool has_input_devices = input_devices_.size() > 0;
  if (has_input_devices) {
    AddAudioSubHeader(container, kSystemMenuAudioInputIcon,
                      IDS_ASH_STATUS_TRAY_AUDIO_INPUT);
  }

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();

  for (const auto& device : input_devices_) {
    CreateLabeledSliderView(container, device);

    // AGC info row is only meaningful when UI gains is going to be ignored.
    if (base::FeatureList::IsEnabled(media::kIgnoreUiGains)) {
      if (audio_handler->GetPrimaryActiveInputNode() == device.id) {
        container->AddChildView(AudioDetailedView::CreateAgcInfoRow(device));
        UpdateAgcInfoRow();
      }
    }

    // Adds the input style transfer toggle.
    if (audio_handler->GetPrimaryActiveInputNode() == device.id &&
        audio_handler->IsStyleTransferSupportedForDevice(device.id)) {
      style_transfer_view_ = container->AddChildView(
          AudioDetailedView::CreateStyleTransferToggleRow(device));

      AddSeparatorIfNotLast(container, device);
    }

    // Adds the input noise cancellation toggle.
    if (audio_handler->GetPrimaryActiveInputNode() == device.id &&
        audio_handler->IsNoiseCancellationSupportedForDevice(device.id)) {
      noise_cancellation_view_ = container->AddChildView(
          AudioDetailedView::CreateNoiseCancellationToggleRow(device));

      AddSeparatorIfNotLast(container, device);
    }

    // Adds a warning message if NBS is selected.
    if (audio_handler->GetPrimaryActiveInputNode() == device.id &&
        device.type == AudioDeviceType::kBluetoothNbMic) {
      container->AddChildView(AudioDetailedView::CreateNbsWarningView());
    }
  }

  container->SizeToPreferredSize();
  scroller()->DeprecatedLayoutImmediately();
}

void AudioDetailedView::UpdateAgcInfoRow() {
  if (!base::FeatureList::IsEnabled(media::kIgnoreUiGains)) {
    return;
  }
  if (!scroll_content()) {
    return;
  }
  HoverHighlightView* agc_info_view = static_cast<HoverHighlightView*>(
      scroll_content()->GetViewByID(AudioDetailedViewID::kAgcInfoView));
  if (!agc_info_view) {
    return;
  }
  views::Label* text_label = agc_info_view->text_label();
  CHECK(text_label);

  std::vector<std::string> app_names = GetNamesOfAppsAccessingMic(
      app_registry_cache_, app_capability_access_cache_);

  std::u16string agc_info_text = GetTextForAgcInfo(app_names);
  text_label->SetText(agc_info_text);

  agc_info_view->GetViewAccessibility().SetName(agc_info_text);
  agc_info_view->SetVisible(ShowAgcInfoRow() && !app_names.empty());
}

bool AudioDetailedView::ShowAgcInfoRow() {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  CHECK(audio_handler);

  // If UI gains is not going to be ignored.
  if (!base::FeatureList::IsEnabled(media::kIgnoreUiGains)) {
    return false;
  }
  // If UI gains is to be force respected.
  if (audio_handler->GetForceRespectUiGainsState()) {
    return false;
  }
  // If there's no stream ignoring UI gains.
  if (num_stream_ignore_ui_gains_ == 0) {
    return false;
  }

  return true;
}

void AudioDetailedView::HandleViewClicked(views::View* view) {
  if (live_caption_view_ && view == live_caption_view_) {
    ToggleLiveCaptionState();
    return;
  }

  if (noise_cancellation_view_ && view == noise_cancellation_view_) {
    OnInputNoiseCancellationTogglePressed();
    return;
  }

  if (style_transfer_view_ && view == style_transfer_view_) {
    OnInputStyleTransferTogglePressed();
    return;
  }

  AudioDeviceViewMap::iterator iter = device_map_.find(view);
  if (iter == device_map_.end()) {
    return;
  }
  AudioDevice device = iter->second;

  // If the clicked view is focused, save the id of this device to preserve the
  // focus ring.
  if (view->HasFocus()) {
    focused_device_id_ = device.id;
  }

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (device.type == AudioDeviceType::kInternalMic &&
      audio_handler->HasDualInternalMic()) {
    audio_handler->SwitchToFrontOrRearMic();
  } else {
    audio_handler->SwitchToDevice(device, true,
                                  DeviceActivateType::kActivateByUser);
  }
}

void AudioDetailedView::CreateExtraTitleRowButtons() {
  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);
  std::unique_ptr<views::Button> settings =
      base::WrapUnique(CreateSettingsButton(
          base::BindRepeating(&AudioDetailedView::OnSettingsButtonClicked,
                              weak_factory_.GetWeakPtr()),
          IDS_ASH_STATUS_TRAY_AUDIO_SETTINGS));
  settings->SetProperty(
      views::kElementIdentifierKey,
      kQuickSettingsAudioDetailedViewAudioSettingsButtonElementId);
  settings_button_ =
      tri_view()->AddView(TriView::Container::END, std::move(settings));
}

// SodaInstaller::Observer:
void AudioDetailedView::OnSodaInstalled(speech::LanguageCode language_code) {
  std::u16string message = l10n_util::GetStringUTF16(
      IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_COMPLETE);
  MaybeShowSodaMessage(language_code, message);
}

void AudioDetailedView::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  std::u16string error_message;
  switch (error_code) {
    case speech::SodaInstaller::ErrorCode::kUnspecifiedError: {
      error_message = l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_ERROR);
      break;
    }
    case speech::SodaInstaller::ErrorCode::kNeedsReboot: {
      error_message = l10n_util::GetStringUTF16(
          IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_ERROR_REBOOT_REQUIRED);
      break;
    }
  }

  MaybeShowSodaMessage(language_code, error_message);
}

void AudioDetailedView::OnSodaProgress(speech::LanguageCode language_code,
                                       int progress) {
  std::u16string message = l10n_util::GetStringFUTF16Int(
      IDS_ASH_ACCESSIBILITY_SETTING_SUBTITLE_SODA_DOWNLOAD_PROGRESS, progress);
  MaybeShowSodaMessage(language_code, message);
}

void AudioDetailedView::OnOutputMuteChanged(bool mute_on) {
  MaybeUpdateActiveDeviceColor(/*is_input=*/false, mute_on, device_map_);
}

void AudioDetailedView::OnInputMuteChanged(
    bool mute_on,
    CrasAudioHandler::InputMuteChangeMethod method) {
  MaybeUpdateActiveDeviceColor(/*is_input=*/true, mute_on, device_map_);
}

void AudioDetailedView::OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) {
  MaybeUpdateActiveDeviceColor(/*is_input=*/true, muted, device_map_);
}

void AudioDetailedView::OnNumStreamIgnoreUiGainsChanged(int32_t num) {
  num_stream_ignore_ui_gains_ = num;
  UpdateAgcInfoRow();
}

BEGIN_METADATA(AudioDetailedView)
END_METADATA

}  // namespace ash
