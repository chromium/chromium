// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_DETAILED_VIEW_H_
#define ASH_SYSTEM_LOCALE_LOCALE_DETAILED_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/containers/flat_map.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// The detailed view to show when the locale feature button is clicked.
// The view shows a list of languages which can be used for demo mode.
// To show this UI on device, see go/demo-mode-g3-cookbook.
// To show this UI in the emulator, pass --qs-show-locale-tile.
class ASH_EXPORT LocaleDetailedView : public TrayDetailedView {
  METADATA_HEADER(LocaleDetailedView, TrayDetailedView)

 public:
  explicit LocaleDetailedView(DetailedViewDelegate* delegate);
  LocaleDetailedView(const LocaleDetailedView&) = delete;
  LocaleDetailedView& operator=(const LocaleDetailedView&) = delete;
  ~LocaleDetailedView() override;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  views::View* GetScrollContentForTest();

 private:
  void CreateItems();

  // The map between the id of the view and the locale it corresponds to.
  base::flat_map<int, std::string> id_to_locale_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_DETAILED_VIEW_H_