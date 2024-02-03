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
#include "base/memory/raw_ptr.h"
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
  METADATA_HEADER(AmbientSlideshowPeripheralUi, views::View)

 public:
  explicit AmbientSlideshowPeripheralUi(AmbientViewDelegate* delegate);
  ~AmbientSlideshowPeripheralUi() override;

  // MediaStringView::Delegate:
  MediaStringView::Settings GetSettings() override;

  // Applies jitter to all elements of the UI using the `jitter_calculator`
  // provided in the constructor. The caller is responsible for invoking this at
  // the desired frequency to prevent screen burn.
  void UpdateGlanceableInfoPosition();

  void UpdateLeftPaddingToMatchBottom();

  void UpdateImageDetails(const std::u16string& details,
                          const std::u16string& related_details);

 private:
  void InitLayout(AmbientViewDelegate* delegate);

  std::unique_ptr<JitterCalculator> jitter_calculator_;

  raw_ptr<AmbientInfoView> ambient_info_view_ = nullptr;

  raw_ptr<MediaStringView> media_string_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_SLIDESHOW_PERIPHERAL_UI_H_
