// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_
#define ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_

#include <map>

#include "ash/system/tray/tray_detailed_view.h"
#include "base/macros.h"
#include "chromeos/audio/audio_device.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {
namespace tray {

class AudioDetailedView : public TrayDetailedView {
 public:
  explicit AudioDetailedView(DetailedViewDelegate* delegate);

  ~AudioDetailedView() override;

  void Update();

  // views::View:
  const char* GetClassName() const override;

 private:
  // Helper function to add non-clickable header rows within the scrollable
  // list.
  void AddAudioSubHeader(const gfx::VectorIcon& icon, int text_id);

  void CreateItems();

  void UpdateScrollableList();
  void UpdateAudioDevices();

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  typedef std::map<views::View*, chromeos::AudioDevice> AudioDeviceMap;

  chromeos::AudioDeviceList output_devices_;
  chromeos::AudioDeviceList input_devices_;
  AudioDeviceMap device_map_;

  DISALLOW_COPY_AND_ASSIGN(AudioDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_AUDIO_DETAILED_VIEW_H_
