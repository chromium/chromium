// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PUBLIC_ACCOUNT_MENU_VIEW_H_
#define ASH_LOGIN_UI_PUBLIC_ACCOUNT_MENU_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/combobox/combobox.h"

namespace ash {

// Implements a menu view for the login screen's expanded public account view.
class ASH_EXPORT PublicAccountMenuView : public views::Combobox {
  METADATA_HEADER(PublicAccountMenuView, views::Combobox)

 public:
  struct Item {
    Item();

    std::string title;
    std::string value;
    bool is_group = false;
    bool selected = false;
  };

  using OnSelect = base::RepeatingCallback<void(const std::string& value)>;

  PublicAccountMenuView(const std::vector<Item>& items,
                        std::optional<size_t> selected_index,
                        const OnSelect& on_select);
  PublicAccountMenuView(const PublicAccountMenuView&) = delete;
  PublicAccountMenuView& operator=(const PublicAccountMenuView&) = delete;
  ~PublicAccountMenuView() override;

  void OnSelectedIndexChanged();

 private:
  const raw_ref<const std::vector<Item>> items_;
  const OnSelect on_select_;

  base::CallbackListSubscription property_changed_subscription_;

  base::WeakPtrFactory<PublicAccountMenuView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PUBLIC_ACCOUNT_MENU_VIEW_H_
