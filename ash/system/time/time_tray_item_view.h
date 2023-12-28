// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_TIME_TRAY_ITEM_VIEW_H_
#define ASH_SYSTEM_TIME_TIME_TRAY_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {
class Shelf;

class ASH_EXPORT TimeTrayItemView : public TrayItemView {
  METADATA_HEADER(TimeTrayItemView, TrayItemView)

 public:
  TimeTrayItemView(Shelf* shelf, TimeView::Type type);

  TimeTrayItemView(const TimeTrayItemView&) = delete;
  TimeTrayItemView& operator=(const TimeTrayItemView&) = delete;

  ~TimeTrayItemView() override;

  void UpdateAlignmentForShelf(Shelf* shelf);
  TimeView* time_view() { return time_view_; }

  // TrayItemView:
  void HandleLocaleChange() override;
  void UpdateLabelOrImageViewColor(bool active) override;

 private:
  friend class TimeTrayItemViewTest;

  raw_ptr<TimeView> time_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_TIME_TRAY_ITEM_VIEW_H_
