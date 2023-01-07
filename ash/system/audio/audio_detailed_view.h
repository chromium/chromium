// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_
#define ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_

#include <map>
#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "base/callback.h"
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

class ASH_EXPORT AudioDetailedView : public TrayDetailedView,
                                     public AccessibilityObserver,
                                     public speech::SodaInstaller::Observer {
 public:
  explicit AudioDetailedView(DetailedViewDelegate* delegate);

  AudioDetailedView(const AudioDetailedView&) = delete;
  AudioDetailedView& operator=(const AudioDetailedView&) = delete;

  ~AudioDetailedView() override;

  void Update();

  // views::View:
  const char* GetClassName() const override;

  using NoiseCancellationCallback =
      base::RepeatingCallback<void(uint64_t, views::View*)>;
  static void SetMapNoiseCancellationToggleCallbackForTest(
      NoiseCancellationCallback* map_noise_cancellation_toggle_callback);

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

 private:
  friend class UnifiedAudioDetailedViewControllerSodaTest;
  friend class UnifiedAudioDetailedViewControllerTest;

  // Helper function to add non-clickable header rows within the scrollable
  // list.
  void AddAudioSubHeader(const gfx::VectorIcon& icon, int text_id);

  void CreateItems();

  void UpdateScrollableList();
  void UpdateAudioDevices();

  void OnInputNoiseCancellationTogglePressed();

  std::unique_ptr<views::View> CreateNoiseCancellationToggleRow(
      const AudioDevice& device);

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int combined_progress) override;

  void MaybeShowSodaMessage(speech::LanguageCode language_code,
                            std::u16string message);

  typedef std::map<views::View*, AudioDevice> AudioDeviceMap;

  std::unique_ptr<MicGainSliderController> mic_gain_controller_;
  AudioDeviceList output_devices_;
  AudioDeviceList input_devices_;
  AudioDeviceMap device_map_;

  HoverHighlightView* live_caption_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_
