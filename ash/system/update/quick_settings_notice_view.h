// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UPDATE_QUICK_SETTINGS_NOTICE_VIEW_H_
#define ASH_SYSTEM_UPDATE_QUICK_SETTINGS_NOTICE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// Base view for label button shown in Quick Settings.
class ASH_EXPORT QuickSettingsNoticeView : public views::LabelButton {
  METADATA_HEADER(QuickSettingsNoticeView, views::LabelButton)

 public:
  QuickSettingsNoticeView(ash::ViewID view_id,
                          QsButtonCatalogName catalog_name,
                          int text_id,
                          const gfx::VectorIcon& icon,
                          views::Button::PressedCallback::Callback callback);
  QuickSettingsNoticeView(const QuickSettingsNoticeView&) = delete;
  QuickSettingsNoticeView& operator=(const QuickSettingsNoticeView&) = delete;
  ~QuickSettingsNoticeView() override;

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Sets a special "narrow" layout which uses a shorter string label.
  void SetNarrowLayout(bool narrow);

 protected:
  // Gets the text id for a short version of the button label.
  // This is used when the view is set to a narrow layout.
  // Defaults to using the initial text id.
  virtual int GetShortTextId() const;

 private:
  const int text_id_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UPDATE_QUICK_SETTINGS_NOTICE_VIEW_H_
