// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_LAUNCHER_SEARCH_IPH_VIEW_H_
#define ASH_APP_LIST_VIEWS_LAUNCHER_SEARCH_IPH_VIEW_H_

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_client.h"
#include "ui/views/view.h"

namespace ash {

class LauncherSearchIphView : public views::View {
 public:
  // Delegate for handling actions of `LauncherSearchIphView`.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Run `query` as a launcher search. `query` is localized.
    virtual void RunLauncherSearchQuery(const std::u16string& query) = 0;
    // Opens Assistant page in the launcher.
    virtual void OpenAssistantPage() = 0;
  };

  enum ViewId {
    kSelf = 1,
    kAssistant,
    // Do not put a new id after `kChipStart`. Numbers after `kChipStart`
    // will be used for chips.
    kChipStart
  };

  LauncherSearchIphView(std::unique_ptr<ScopedIphSession> scoped_iph_session,
                        raw_ptr<Delegate> delegate);
  ~LauncherSearchIphView() override;

 private:
  // TODO(b/272370530): Use string id for internationalization.
  void RunLauncherSearchQuery(const std::u16string& query);

  void OpenAssistantPage();

  std::unique_ptr<ScopedIphSession> scoped_iph_session_;
  raw_ptr<Delegate> delegate_;

  base::WeakPtrFactory<LauncherSearchIphView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_LAUNCHER_SEARCH_IPH_VIEW_H_
