// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_
#define ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_

#include <cstdint>
#include <map>
#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/style/switch.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/soda/soda_installer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {
class MicGainSliderController;
class UnifiedAudioDetailedViewControllerSodaTest;
class UnifiedAudioDetailedViewControllerTest;
class UnifiedVolumeSliderController;

class ASH_EXPORT AudioDetailedView
    : public AccessibilityObserver,
      public apps::AppCapabilityAccessCache::Observer,
      public CrasAudioHandler::AudioObserver,
      public SessionObserver,
      public speech::SodaInstaller::Observer,
      public TrayDetailedView {
 public:
  METADATA_HEADER(AudioDetailedView);

  explicit AudioDetailedView(DetailedViewDelegate* delegate);

  AudioDetailedView(const AudioDetailedView&) = delete;
  AudioDetailedView& operator=(const AudioDetailedView&) = delete;

  ~AudioDetailedView() override;

  // IDs used for the views that compose the Audio UI.
  // Note that these IDs are only guaranteed to be unique inside
  // `AudioDetailedView`.
  enum AudioDetailedViewID {
    // Starts at 1000 to prevent potential overlapping.
    kAudioDetailedView = 1000,
    // Agc information row and corresponding text label.
    kAgcInfoRow,
    kAgcInfoLabel,
    // For QsRevamp: AGC information row.
    kAgcInfoView,
    // Warning message view when an NBS device is selected.
    kNbsWarningView,
  };

  using NoiseCancellationCallback =
      base::RepeatingCallback<void(uint64_t, views::View*)>;
  static void SetMapNoiseCancellationToggleCallbackForTest(
      NoiseCancellationCallback* map_noise_cancellation_toggle_callback);

  views::View* GetAsView();

  // Updates the `AudioDetailedView` and re-layout.
  void Update();

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // apps::AppCapabilityAccessCache::Observer:
  void OnCapabilityAccessUpdate(
      const apps::CapabilityAccessUpdate& update) override;
  void OnAppCapabilityAccessCacheWillBeDestroyed(
      apps::AppCapabilityAccessCache* cache) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // CrasAudioHandler::AudioObserver:
  void OnNumStreamIgnoreUiGainsChanged(int32_t num) override;

 private:
  friend class AudioDetailedViewTest;
  friend class AudioDetailedViewAgcInfoTest;
  friend class UnifiedAudioDetailedViewControllerSodaTest;
  friend class UnifiedAudioDetailedViewControllerTest;

  // Helper function to add non-clickable header rows within the scrollable
  // list.
  void AddAudioSubHeader(views::View* container,
                         const gfx::VectorIcon& icon,
                         const int text_id);

  // For QsRevamp: Adds the sliders for output/input devices.
  views::View* AddDeviceSlider(views::View* container,
                               const AudioDevice& device,
                               HoverHighlightView* device_name_container,
                               bool is_output_device);

  // Creates the items other than the devices during initialization.
  void CreateItems();

  // Creates the NBS warning view.
  std::unique_ptr<TriView> CreateNbsWarningView();

  // For QsRevamp: Creates the `live_caption_view_`.
  void CreateLiveCaptionView();

  // Creates the noise cancellation toggle row in the input subsection.
  std::unique_ptr<views::View> CreateNoiseCancellationToggleRow(
      const AudioDevice& device);

  // For QsRevamp: Creates the noise cancellation toggle row in the input
  // subsection.
  std::unique_ptr<HoverHighlightView> CreateQsNoiseCancellationToggleRow(
      const AudioDevice& device);

  // Creates the agc info row in the input subsection.
  views::Builder<views::BoxLayoutView> CreateAgcInfoRow(
      const AudioDevice& device);

  // For QsRevamp: Creates the agc info row in the input subsection.
  std::unique_ptr<HoverHighlightView> CreateQsAgcInfoRow(
      const AudioDevice& device);

  // Sets the subtext for `live_caption_view_` based on whether live caption has
  // updated if this feature is enabled and visible in tray.
  void MaybeShowSodaMessage(speech::LanguageCode language_code,
                            std::u16string message);

  // Callback passed to the noise cancellation toggle button.
  void OnInputNoiseCancellationTogglePressed();

  // Callback passed to the Settings button.
  void OnSettingsButtonClicked();

  // Toggles live caption state to trigger `AccessibilityObserver` to update the
  // UI.
  void ToggleLiveCaptionState();

  // Updates `live_caption_view_` via the UI based on `is_enabled`.
  void UpdateLiveCaptionView(bool is_enabled);

  // Updates `output_devices_` and `input_devices_`.
  void UpdateAudioDevices();

  // Updates the child views in `scroll_content()`.
  void UpdateScrollableList();

  // Updates the label and checkmark color of `device_name_container` based on
  // whether this device is muted or not.
  void UpdateDeviceContainerColor(HoverHighlightView* device_name_container,
                                  bool is_muted);

  // Callback to change the active node's color based on the mute state. Gets
  // called when the input/output node's mute state changes.
  void UpdateActiveDeviceColor(bool is_input, bool is_muted);

  // Updates the label of AGC info when accessibility to microphone changed.
  // Hide AGC info row if no apps is requesting AGC stream.
  void UpdateAgcInfoRow();
  void UpdateQsAgcInfoRow();
  bool ShowAgcInfoRow();

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void CreateExtraTitleRowButtons() override;

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int combined_progress) override;

  // CrasAudioHandler::AudioObserver:
  void OnOutputMuteChanged(bool mute_on) override;
  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;
  void OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) override;

  typedef std::map<views::View*, AudioDevice> AudioDeviceMap;

  std::unique_ptr<MicGainSliderController> mic_gain_controller_;
  std::unique_ptr<UnifiedVolumeSliderController>
      unified_volume_slider_controller_;
  AudioDeviceList output_devices_;
  AudioDeviceList input_devices_;
  AudioDeviceMap device_map_;
  uint64_t focused_device_id_ = -1;

  int num_stream_ignore_ui_gains_ = 0;

  // Owned by the views hierarchy.
  raw_ptr<HoverHighlightView, ExperimentalAsh> live_caption_view_ = nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> live_caption_icon_ = nullptr;
  raw_ptr<Switch, ExperimentalAsh> live_caption_button_ = nullptr;
  raw_ptr<HoverHighlightView, ExperimentalAsh> noise_cancellation_view_ =
      nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> noise_cancellation_icon_ = nullptr;
  raw_ptr<Switch, ExperimentalAsh> noise_cancellation_button_ = nullptr;
  raw_ptr<views::Button, ExperimentalAsh> settings_button_ = nullptr;

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};
  base::ScopedObservation<apps::AppCapabilityAccessCache,
                          apps::AppCapabilityAccessCache::Observer>
      app_capability_observation_{this};
  raw_ptr<apps::AppRegistryCache> app_registry_cache_;
  raw_ptr<apps::AppCapabilityAccessCache> app_capability_access_cache_;

  base::WeakPtrFactory<AudioDetailedView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_
