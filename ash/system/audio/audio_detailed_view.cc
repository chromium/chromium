// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_detailed_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/audio/mic_gain_slider_view.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "ash/system/tray/tri_view.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

const int kLabelFontSizeDelta = 1;
const int kToggleButtonRowViewSpacing = 18;
constexpr auto kToggleButtonRowLabelPadding = gfx::Insets::TLBR(16, 0, 15, 0);
constexpr auto kToggleButtonRowViewPadding = gfx::Insets::TLBR(0, 56, 8, 0);

// This callback is only used for tests.
AudioDetailedView::NoiseCancellationCallback*
    g_noise_cancellation_toggle_callback = nullptr;

std::u16string GetAudioDeviceName(const AudioDevice& device) {
  switch (device.type) {
    case AudioDeviceType::kFrontMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_FRONT_MIC);
    case AudioDeviceType::kHeadphone:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HEADPHONE);
    case AudioDeviceType::kInternalSpeaker:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_SPEAKER);
    case AudioDeviceType::kInternalMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INTERNAL_MIC);
    case AudioDeviceType::kRearMic:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_REAR_MIC);
    case AudioDeviceType::kUsb:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_USB_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kBluetooth:
      [[fallthrough]];
    case AudioDeviceType::kBluetoothNbMic:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_BLUETOOTH_DEVICE,
          base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kHdmi:
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_AUDIO_HDMI_DEVICE,
                                        base::UTF8ToUTF16(device.display_name));
    case AudioDeviceType::kMic:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_MIC_JACK_DEVICE);
    default:
      return base::UTF8ToUTF16(device.display_name);
  }
}

speech::LanguageCode GetLiveCaptionLocale() {
  std::string live_caption_locale = speech::kUsEnglishLocale;
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (pref_service) {
    live_caption_locale = ::prefs::GetLiveCaptionLanguageCode(pref_service);
  }
  return speech::GetLanguageCode(live_caption_locale);
}

}  // namespace

AudioDetailedView::AudioDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  CreateItems();

  Shell::Get()->accessibility_controller()->AddObserver(this);

  if (!captions::IsLiveCaptionFeatureSupported())
    return;
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (soda_installer)
    soda_installer->AddObserver(this);
}

AudioDetailedView::~AudioDetailedView() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
  if (!captions::IsLiveCaptionFeatureSupported())
    return;
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  // `soda_installer` is not guaranteed to be valid, since it's possible for
  // this class to out-live it. This means that this class cannot use
  // ScopedObservation and needs to manage removing the observer itself.
  if (soda_installer)
    soda_installer->RemoveObserver(this);
}

void AudioDetailedView::Update() {
  UpdateAudioDevices();
  Layout();
}

const char* AudioDetailedView::GetClassName() const {
  return "AudioDetailedView";
}

void AudioDetailedView::AddAudioSubHeader(const gfx::VectorIcon& icon,
                                          int text_id) {
  TriView* header = AddScrollListSubHeader(scroll_content(), icon, text_id);
  header->SetContainerVisible(TriView::Container::END, false);
}

void AudioDetailedView::CreateItems() {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_AUDIO);
  mic_gain_controller_ = std::make_unique<MicGainSliderController>();
}

void AudioDetailedView::UpdateAudioDevices() {
  output_devices_.clear();
  input_devices_.clear();
  AudioDeviceList devices;
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  audio_handler->GetAudioDevices(&devices);
  bool has_dual_internal_mic = audio_handler->HasDualInternalMic();
  bool is_front_or_rear_mic_active = false;
  for (const auto& device : devices) {
    // Don't display keyboard mic or aokr type.
    if (!device.is_for_simple_usage())
      continue;
    if (device.is_input) {
      // Do not expose the internal front and rear mic to UI.
      if (has_dual_internal_mic && audio_handler->IsFrontOrRearMic(device)) {
        if (device.active)
          is_front_or_rear_mic_active = true;
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
    internal_mic.stable_device_id_version = 2;
    internal_mic.type = AudioDeviceType::kInternalMic;
    internal_mic.active = is_front_or_rear_mic_active;
    input_devices_.push_back(internal_mic);
  }

  UpdateScrollableList();
}

void AudioDetailedView::UpdateScrollableList() {
  scroll_content()->RemoveAllChildViews();
  device_map_.clear();

  // Add live caption toggle.
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  if (controller->IsLiveCaptionSettingVisibleInTray()) {
    live_caption_view_ = AddScrollListCheckableItem(
        scroll_content(), vector_icons::kLiveCaptionOnIcon,
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION),
        controller->live_caption().enabled(),
        controller->IsEnterpriseIconVisibleForLiveCaption());
    scroll_content()->AddChildView(
        TrayPopupUtils::CreateListSubHeaderSeparator());
  }

  // Add audio output devices.
  const bool has_output_devices = output_devices_.size() > 0;
  if (has_output_devices) {
    AddAudioSubHeader(kSystemMenuAudioOutputIcon,
                      IDS_ASH_STATUS_TRAY_AUDIO_OUTPUT);
  }

  for (const auto& device : output_devices_) {
    HoverHighlightView* container =
        AddScrollListCheckableItem(scroll_content(), gfx::kNoneIcon,
                                   GetAudioDeviceName(device), device.active);
    device_map_[container] = device;
  }

  if (has_output_devices) {
    scroll_content()->AddChildView(
        TrayPopupUtils::CreateListSubHeaderSeparator());
  }

  // Add audio input devices.
  const bool has_input_devices = input_devices_.size() > 0;
  if (has_input_devices) {
    AddAudioSubHeader(kSystemMenuAudioInputIcon,
                      IDS_ASH_STATUS_TRAY_AUDIO_INPUT);
  }

  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();

  // Set the input noise cancellation state.
  if (audio_handler->noise_cancellation_supported()) {
    for (const auto& device : input_devices_) {
      if (device.type == AudioDeviceType::kInternalMic) {
        audio_handler->SetNoiseCancellationState(
            audio_handler->GetNoiseCancellationState() &&
            (device.audio_effect & cras::EFFECT_TYPE_NOISE_CANCELLATION));
        break;
      }
    }
  }

  for (const auto& device : input_devices_) {
    HoverHighlightView* container =
        AddScrollListCheckableItem(scroll_content(), gfx::kNoneIcon,
                                   GetAudioDeviceName(device), device.active);
    device_map_[container] = device;

    // Add the input noise cancellation toggle.
    if (audio_handler->GetPrimaryActiveInputNode() == device.id &&
        audio_handler->noise_cancellation_supported()) {
      if (device.audio_effect & cras::EFFECT_TYPE_NOISE_CANCELLATION) {
        scroll_content()->AddChildView(
            AudioDetailedView::CreateNoiseCancellationToggleRow(device));
      }
    }

    scroll_content()->AddChildView(mic_gain_controller_->CreateMicGainSlider(
        device.id, device.IsInternalMic()));
  }

  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
}

std::unique_ptr<views::View>
AudioDetailedView::CreateNoiseCancellationToggleRow(const AudioDevice& device) {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  auto noise_cancellation_toggle = std::make_unique<TrayToggleButton>(
      base::BindRepeating(
          &AudioDetailedView::OnInputNoiseCancellationTogglePressed,
          base::Unretained(this)),
      IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION);

  noise_cancellation_toggle->SetIsOn(
      audio_handler->GetNoiseCancellationState());

  auto noise_cancellation_toggle_row = std::make_unique<views::View>();

  auto* row_layout = noise_cancellation_toggle_row->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kToggleButtonRowViewPadding, kToggleButtonRowViewSpacing));

  noise_cancellation_toggle->SetFlipCanvasOnPaintForRTLUI(false);

  auto noise_cancellation_label =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION));

  noise_cancellation_label->SetEnabledColorId(kColorAshTextColorPrimary);
  noise_cancellation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  noise_cancellation_label->SetFontList(
      gfx::FontList().DeriveWithSizeDelta(kLabelFontSizeDelta));
  noise_cancellation_label->SetAutoColorReadabilityEnabled(false);
  noise_cancellation_label->SetSubpixelRenderingEnabled(false);
  noise_cancellation_label->SetBorder(
      views::CreateEmptyBorder(kToggleButtonRowLabelPadding));

  auto* label_ptr = noise_cancellation_toggle_row->AddChildView(
      std::move(noise_cancellation_label));
  row_layout->SetFlexForView(label_ptr, 1);

  noise_cancellation_toggle_row->AddChildView(
      std::move(noise_cancellation_toggle));

  // This is only used for testing.
  if (g_noise_cancellation_toggle_callback) {
    g_noise_cancellation_toggle_callback->Run(
        device.id, noise_cancellation_toggle_row.get());
  }

  return noise_cancellation_toggle_row;
}

void AudioDetailedView::SetMapNoiseCancellationToggleCallbackForTest(
    AudioDetailedView::NoiseCancellationCallback*
        noise_cancellation_toggle_callback) {
  g_noise_cancellation_toggle_callback = noise_cancellation_toggle_callback;
}

void AudioDetailedView::OnInputNoiseCancellationTogglePressed() {
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  const bool new_state = !audio_handler->GetNoiseCancellationState();
  audio_handler->SetNoiseCancellationState(new_state);
  audio_handler->SetNoiseCancellationPrefState(new_state);
}

void AudioDetailedView::HandleViewClicked(views::View* view) {
  if (live_caption_view_ && view == live_caption_view_) {
    AccessibilityControllerImpl* controller =
        Shell::Get()->accessibility_controller();
    controller->live_caption().SetEnabled(
        !controller->live_caption().enabled());
    return;
  }

  AudioDeviceMap::iterator iter = device_map_.find(view);
  if (iter == device_map_.end())
    return;
  AudioDevice device = iter->second;
  CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
  if (device.type == AudioDeviceType::kInternalMic &&
      audio_handler->HasDualInternalMic()) {
    audio_handler->SwitchToFrontOrRearMic();
  } else {
    audio_handler->SwitchToDevice(device, true,
                                  CrasAudioHandler::ACTIVATE_BY_USER);
  }
}

void AudioDetailedView::OnAccessibilityStatusChanged() {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  if (live_caption_view_ && controller->IsLiveCaptionSettingVisibleInTray()) {
    TrayPopupUtils::UpdateCheckMarkVisibility(
        live_caption_view_, controller->live_caption().enabled());
  }
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

void AudioDetailedView::MaybeShowSodaMessage(speech::LanguageCode language_code,
                                             std::u16string message) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  bool is_live_caption_enabled = controller->live_caption().enabled();
  bool is_live_caption_in_tray =
      live_caption_view_ && controller->IsLiveCaptionSettingVisibleInTray();
  // Only show updates for this feature if the language code applies to the SODA
  // binary (encoded by by LanguageCode::kNone) or the language pack matching
  // the feature locale.
  bool live_caption_has_update = language_code == speech::LanguageCode::kNone ||
                                 language_code == GetLiveCaptionLocale();
  if (is_live_caption_enabled && is_live_caption_in_tray &&
      live_caption_has_update) {
    live_caption_view_->SetSubText(message);
  }
}

}  // namespace ash
