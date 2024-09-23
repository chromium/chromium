// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_AUTOZOOM_TOAST_VIEW_H_
#define ASH_SYSTEM_CAMERA_AUTOZOOM_TOAST_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

class FeaturePodIconButton;
class AutozoomToastController;

// The view shown inside the autozoom toast bubble.
class ASH_EXPORT AutozoomToastView : public views::View,
                                     public views::ViewObserver {
  METADATA_HEADER(AutozoomToastView, views::View)

 public:
  explicit AutozoomToastView(AutozoomToastController* controller);
  AutozoomToastView(AutozoomToastView&) = delete;
  AutozoomToastView operator=(AutozoomToastView&) = delete;
  ~AutozoomToastView() override;

  // Updates the toast with whether autozoom is enabled.
  void SetAutozoomEnabled(bool enabled);

  // Returns true if the toggle button is focused.
  bool IsButtonFocused() const;

  std::u16string accessible_name() const;

 private:
  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  raw_ptr<AutozoomToastController> controller_ = nullptr;
  raw_ptr<FeaturePodIconButton> button_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_AUTOZOOM_TOAST_VIEW_H_
