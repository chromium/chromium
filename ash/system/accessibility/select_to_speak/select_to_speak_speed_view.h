// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_SPEED_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_SPEED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

// View for the Select-to-Speak reading speed (speech rate) selection list.
class SelectToSpeakSpeedView : public views::BoxLayoutView,
                               public ViewClickListener {
  METADATA_HEADER(SelectToSpeakSpeedView, views::BoxLayoutView)

 public:
  class ASH_EXPORT Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual void OnSpeechRateSelected(double speech_rate) = 0;
  };

  SelectToSpeakSpeedView(Delegate* delegate, double initial_speech_rate);
  SelectToSpeakSpeedView(const SelectToSpeakSpeedView&) = delete;
  SelectToSpeakSpeedView& operator=(const SelectToSpeakSpeedView&) = delete;
  ~SelectToSpeakSpeedView() override = default;

  void SetInitialFocus();

  // Sets the speech rate that should be selected.
  void SetInitialSpeechRate(double initial_speech_rate);

 private:
  void AddMenuItem(int option_id,
                   const std::u16string& label,
                   bool is_selected);

  // ViewClickListener:
  void OnViewClicked(views::View* sender) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* key_event) override;

  raw_ptr<Delegate> delegate_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   SelectToSpeakSpeedView,
                   views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SelectToSpeakSpeedView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_SPEED_VIEW_H_
