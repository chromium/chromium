// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_DETAILED_VIEW_H_
#define ASH_SYSTEM_LOCALE_LOCALE_DETAILED_VIEW_H_

#include <string>

#include "ash/system/tray/tray_detailed_view.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"

namespace ash {
namespace tray {

// The detailed view to show when the locale feature button is clicked.
class LocaleDetailedView : public TrayDetailedView {
 public:
  explicit LocaleDetailedView(DetailedViewDelegate* delegate);
  ~LocaleDetailedView() override;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  void CreateItems();

  // The map between the id of the view and the locale it corresponds to.
  base::flat_map<int, std::string> id_to_locale_;

  DISALLOW_COPY_AND_ASSIGN(LocaleDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_DETAILED_VIEW_H_