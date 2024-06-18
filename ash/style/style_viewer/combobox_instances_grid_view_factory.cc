// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/style/combobox.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr size_t kGridViewRowNum = 2;
constexpr size_t kGridViewColNum = 2;
constexpr size_t kGridViewRowGroupSize = 1;
constexpr size_t kGirdViewColGroupSize = 2;

// A test combobox model.
class ComboboxModelExample : public ui::ComboboxModel {
 public:
  explicit ComboboxModelExample(size_t item_num) : item_num_(item_num) {}
  ComboboxModelExample(const ComboboxModelExample&) = delete;
  ComboboxModelExample& operator=(const ComboboxModelExample&) = delete;
  ~ComboboxModelExample() override = default;

 private:
  // ui::ComboboxModel:
  size_t GetItemCount() const override { return item_num_; }

  std::u16string GetItemAt(size_t index) const override {
    CHECK_LT(index, item_num_);
    return base::UTF8ToUTF16(base::StringPrintf(
        "%zu. %c item", index, static_cast<char>('A' + index)));
  }

  const size_t item_num_;
};

// A test system UI components grid view with a combobox model.
class ComboboxExampleGridView : public SystemUIComponentsGridView {
 public:
  explicit ComboboxExampleGridView(size_t item_size)
      : SystemUIComponentsGridView(kGridViewRowNum,
                                   kGridViewColNum,
                                   kGridViewRowGroupSize,
                                   kGirdViewColGroupSize),
        model_(std::make_unique<ComboboxModelExample>(item_size)) {}
  ComboboxExampleGridView(const ComboboxExampleGridView&) = delete;
  ComboboxExampleGridView& operator=(const ComboboxExampleGridView&) = delete;
  ~ComboboxExampleGridView() override = default;

  ui::ComboboxModel* model() const { return model_.get(); }

 private:
  std::unique_ptr<ComboboxModelExample> model_;
};

}  // namespace

std::unique_ptr<SystemUIComponentsGridView> CreateComboboxInstancesGridView() {
  auto grid_view = std::make_unique<ComboboxExampleGridView>(3);

  // A callback to update the `label` on `combobox` selection changes.
  auto update_label_text = [](Combobox* combobox, views::Label* label) {
    label->SetText(
        u"Item " +
        base::NumberToString16(combobox->GetSelectedIndex().value()) +
        u" is selected.");
  };

  // An example of combobox with owned model.
  auto* combobox_owned_model = grid_view->AddInstance(
      u"Combobox with owned model",
      std::make_unique<Combobox>(std::make_unique<ComboboxModelExample>(4)));
  combobox_owned_model->SetTooltipText(u"Combobox with owned model");
  auto* label_for_owned_model =
      grid_view->AddInstance(u"", std::make_unique<views::Label>());
  update_label_text(combobox_owned_model, label_for_owned_model);
  combobox_owned_model->SetSelectionChangedCallback(base::BindRepeating(
      update_label_text, base::Unretained(combobox_owned_model),
      base::Unretained(label_for_owned_model)));

  // An example of combobox with unowned model.
  auto* combobox_non_owned_model =
      grid_view->AddInstance(u"Combobox with non-owned model",
                             std::make_unique<Combobox>(grid_view->model()));
  combobox_non_owned_model->SetTooltipText(u"Combobox with non-owned model");
  auto* label_for_non_owned_model =
      grid_view->AddInstance(u"", std::make_unique<views::Label>());
  update_label_text(combobox_non_owned_model, label_for_non_owned_model);
  combobox_non_owned_model->SetSelectionChangedCallback(base::BindRepeating(
      update_label_text, base::Unretained(combobox_non_owned_model),
      base::Unretained(label_for_non_owned_model)));
  return grid_view;
}

}  // namespace ash
