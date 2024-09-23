// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_BAR_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class CaptureModeSourceView;
class CaptureModeTypeView;
class PillButton;
class IconButton;
class SystemShadow;

// The contents of the capture bar can change based on the session initiation
// type. Different clients of capture mode require different capture mode bar.
// See `CaptureModeBehavior`.
class ASH_EXPORT CaptureModeBarView : public views::View {
  METADATA_HEADER(CaptureModeBarView, views::View)

 public:
  ~CaptureModeBarView() override;

  IconButton* settings_button() const { return settings_button_; }
  IconButton* close_button() const { return close_button_; }

  // These functions may return `nullptr` depending on the actual type of the
  // bar.
  virtual CaptureModeTypeView* GetCaptureTypeView() const;
  virtual CaptureModeSourceView* GetCaptureSourceView() const;
  virtual PillButton* GetStartRecordingButton() const;

  // Called when either the capture mode source or type changes.
  virtual void OnCaptureSourceChanged(CaptureModeSource new_source);
  virtual void OnCaptureTypeChanged(CaptureModeType new_type);

  // Called when settings is toggled on or off.
  virtual void SetSettingsMenuShown(bool shown);

  bool IsEventOnSettingsButton(gfx::Point screen_location) const;

  // views::View:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 protected:
  CaptureModeBarView();

  // Adds the common elements of different capture bars to the bar view.
  void AppendSettingsButton();
  void AppendCloseButton();

 private:
  void OnSettingsButtonPressed(const ui::Event& event);
  void OnCloseButtonPressed();

  raw_ptr<IconButton> settings_button_ = nullptr;
  raw_ptr<IconButton> close_button_ = nullptr;
  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_BAR_VIEW_H_
