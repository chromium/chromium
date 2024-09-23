// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_LABELED_SLIDER_VIEW_H_
#define ASH_SYSTEM_AUDIO_LABELED_SLIDER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

struct AudioDevice;
class HoverHighlightView;
class TrayDetailedView;
class QuickSettingsSlider;
class UnifiedSliderView;

// Generates a slider labeled with the device name for an output/input device.
// - If `is_wide_slider` is true, applies smaller padding for a wider slider
// appearance.
// - Otherwise, uses the default padding values defined in `UnifiedVolumeView`.
class ASH_EXPORT LabeledSliderView : public views::View {
  METADATA_HEADER(LabeledSliderView, views::View)

 public:
  LabeledSliderView(TrayDetailedView* detailed_view,
                    std::unique_ptr<views::View> slider_view,
                    const AudioDevice& device,
                    bool is_wide_slider);

  LabeledSliderView(const LabeledSliderView&) = delete;
  LabeledSliderView& operator=(const LabeledSliderView&) = delete;
  ~LabeledSliderView() override;

  HoverHighlightView* device_name_view() const { return device_name_view_; }
  UnifiedSliderView* unified_slider_view() const {
    return unified_slider_view_;
  }

 private:
  // Applies the appropriate color, size, and other formatting to the
  // `device_name_view_` depending on the device's active state and the
  // `is_wide_slider_` flag.
  void ConfigureDeviceNameView(const AudioDevice& device);

  // Applies the appropriate focus ring to the `device_name_view_` depending on
  // the device's active state.
  void ConfigureFocusBehavior(const bool is_active,
                              QuickSettingsSlider* slider);

  const bool is_wide_slider_;
  raw_ptr<HoverHighlightView> device_name_view_ = nullptr;
  raw_ptr<UnifiedSliderView> unified_slider_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_LABELED_SLIDER_VIEW_H_
