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
#include "ash/system/tray/tray_detailed_view.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "components/soda/soda_installer.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {
class MicGainSliderController;
class UnifiedAudioDetailedViewControllerSodaTest;
class UnifiedAudioDetailedViewControllerTest;
class UnifiedVolumeSliderController;

class ASH_EXPORT AudioDetailedView : public TrayDetailedView,
                                     public AccessibilityObserver,
                                     public speech::SodaInstaller::Observer {
 public:
  METADATA_HEADER(AudioDetailedView);

  explicit AudioDetailedView(DetailedViewDelegate* delegate);

  AudioDetailedView(const AudioDetailedView&) = delete;
  AudioDetailedView& operator=(const AudioDetailedView&) = delete;

  ~AudioDetailedView() override;

  using NoiseCancellationCallback =
      base::RepeatingCallback<void(uint64_t, views::View*)>;
  static void SetMapNoiseCancellationToggleCallbackForTest(
      NoiseCancellationCallback* map_noise_cancellation_toggle_callback);

  // Updates the `AudioDetailedView` and re-layout.
  void Update();

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

 private:
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

  // For QsRevamp: Creates the `live_caption_view_`.
  void CreateLiveCaptionView();

  // Creates the noise cancellation toggle row in the input subsection.
  std::unique_ptr<views::View> CreateNoiseCancellationToggleRow(
      const AudioDevice& device);

  // Sets the subtext for `live_caption_view_` based on whether live caption has
  // updated if this feature is enabled and visible in tray.
  void MaybeShowSodaMessage(speech::LanguageCode language_code,
                            std::u16string message);

  // Callback passed to the noise cancellation toggle button.
  void OnInputNoiseCancellationTogglePressed();

  // Toggles live caption state to trigger `AccessibilityObserver` to update the
  // UI.
  void ToggleLiveCaptionState();

  // Updates `live_caption_view_` via the UI based on `is_enabled`.
  void UpdateLiveCaptionView(bool is_enabled);

  // Updates `output_devices_` and `input_devices_`.
  void UpdateAudioDevices();

  // Updates the child views in `scroll_content()`.
  void UpdateScrollableList();

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int combined_progress) override;

  typedef std::map<views::View*, AudioDevice> AudioDeviceMap;

  std::unique_ptr<MicGainSliderController> mic_gain_controller_;
  std::unique_ptr<UnifiedVolumeSliderController>
      unified_volume_slider_controller_;
  AudioDeviceList output_devices_;
  AudioDeviceList input_devices_;
  AudioDeviceMap device_map_;
  uint64_t focused_device_id_ = -1;
  // Owned by the views hierarchy.
  HoverHighlightView* live_caption_view_ = nullptr;
  views::ImageView* toggle_icon_ = nullptr;
  views::ToggleButton* toggle_button_ = nullptr;

  base::WeakPtrFactory<AudioDetailedView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_
