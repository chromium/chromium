// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/public_account_menu_view.h"

#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"

namespace ash {

constexpr int kMenuItemWidthDp = 192;

namespace {

class PublicAccountComboboxModel : public ui::ComboboxModel {
 public:
  PublicAccountComboboxModel(
      const std::vector<PublicAccountMenuView::Item>& items,
      std::optional<size_t> default_index)
      : items_(items), default_index_(default_index) {}

  PublicAccountComboboxModel(const PublicAccountComboboxModel&) = delete;
  PublicAccountComboboxModel& operator=(const PublicAccountComboboxModel&) =
      delete;

  ~PublicAccountComboboxModel() override = default;

  // ui::ComboboxModel:
  size_t GetItemCount() const override { return items_->size(); }

  // ui::ComboboxModel:
  std::u16string GetItemAt(size_t index) const override {
    return base::UTF8ToUTF16((*items_)[index].title);
  }

  // ui::ComboboxModel:
  // Alternatively, we could override `IsItemSeparatorAt`, especially since
  // group items are considered as some sort of separators. We choose to
  // represent them as disabled items because they were presented in a similar
  // fashion before (i.e. the group name was visible but unclickable).
  bool IsItemEnabledAt(size_t index) const override {
    return !(*items_)[index].is_group;
  }

  // ui::ComboboxModel:
  std::optional<size_t> GetDefaultIndex() const override {
    return default_index_;
  }

 private:
  const raw_ref<const std::vector<PublicAccountMenuView::Item>> items_;
  const std::optional<size_t> default_index_;
};

}  // namespace

PublicAccountMenuView::Item::Item() = default;

PublicAccountMenuView::PublicAccountMenuView(
    const std::vector<Item>& items,
    std::optional<size_t> selected_index,
    const OnSelect& on_select)
    : views::Combobox(
          std::make_unique<PublicAccountComboboxModel>(items, selected_index)),
      items_(items),
      on_select_(on_select) {
  SetPreferredSize(
      gfx::Size(kMenuItemWidthDp, GetHeightForWidth(kMenuItemWidthDp)));

  property_changed_subscription_ = AddSelectedIndexChangedCallback(
      base::BindRepeating(&PublicAccountMenuView::OnSelectedIndexChanged,
                          weak_factory_.GetWeakPtr()));
}

PublicAccountMenuView::~PublicAccountMenuView() = default;

void PublicAccountMenuView::OnSelectedIndexChanged() {
  on_select_.Run((*items_)[GetSelectedIndex().value()].value);
}

BEGIN_METADATA(PublicAccountMenuView)
END_METADATA

}  // namespace ash
