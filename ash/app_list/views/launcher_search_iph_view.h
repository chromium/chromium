// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_LAUNCHER_SEARCH_IPH_VIEW_H_
#define ASH_APP_LIST_VIEWS_LAUNCHER_SEARCH_IPH_VIEW_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_client.h"
#include "ui/views/view.h"

namespace ash {

class LauncherSearchIphView : public views::View {
 public:
  static constexpr int kViewId = 1;

  explicit LauncherSearchIphView(
      std::unique_ptr<ScopedIphSession> scoped_iph_session);
  ~LauncherSearchIphView() override;

 private:
  std::unique_ptr<ScopedIphSession> scoped_iph_session_;

  base::WeakPtrFactory<LauncherSearchIphView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_LAUNCHER_SEARCH_IPH_VIEW_H_
