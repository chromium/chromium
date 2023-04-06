// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_SLIDESHOW_PERIPHERAL_UI_H_
#define ASH_AMBIENT_UI_AMBIENT_SLIDESHOW_PERIPHERAL_UI_H_

#include <string>

#include "ash/ambient/ui/ambient_info_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class AmbientInfoView;
class JitterCalculator;

// AmbientSlideshowPeripheralUi-----------------------------------------------
// A custom view that shows peripheral elements such as related info, time,
// weather and media string that are shown in the slideshow's photo view.
class AmbientSlideshowPeripheralUi : public views::View,
                                     public MediaStringView::Delegate {
 public:
  METADATA_HEADER(AmbientSlideshowPeripheralUi);

  AmbientSlideshowPeripheralUi(
      AmbientViewDelegate* delegate,
      JitterCalculator* glanceable_info_jitter_calculator);

  ~AmbientSlideshowPeripheralUi() override;

  // MediaStringView::Delegate:
  MediaStringView::Settings GetSettings() override;

  void UpdateGlanceableInfoPosition();

  void UpdateImageDetails(const std::u16string& details,
                          const std::u16string& related_details);

 private:
  void InitLayout(AmbientViewDelegate* delegate);

  const base::raw_ptr<JitterCalculator> glanceable_info_jitter_calculator_;

  AmbientInfoView* ambient_info_view_ = nullptr;

  MediaStringView* media_string_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_SLIDESHOW_PERIPHERAL_UI_H_
