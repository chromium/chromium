// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"
#include "ash/style/system_textfield.h"
#include "ash/style/system_textfield_controller.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

namespace {

// Configurations of grid view for `SystemTextfield` instances.
constexpr size_t kGridViewRowNum = 4;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 4;
constexpr size_t kGirdViewColGroupSize = 1;

}  // namespace

// A component grid view class which allows to add a system textfield with
// system textfield controller. It manages the unique pointers of textfield
// controller.
class TextfieldGridView : public SystemUIComponentsGridView {
 public:
  TextfieldGridView()
      : SystemUIComponentsGridView(kGridViewRowNum,
                                   kGridViewColNum,
                                   kGridViewRowGroupSize,
                                   kGirdViewColGroupSize) {}
  TextfieldGridView(const TextfieldGridView&) = delete;
  TextfieldGridView& operator=(const TextfieldGridView&) = delete;
  ~TextfieldGridView() override = default;

  // Adds a textfield with `SystemTextController` as its controller.
  void AddTextfieldWithController(const std::u16string& name,
                                  std::unique_ptr<SystemTextfield> textfield) {
    controllers_.emplace_back(
        std::make_unique<SystemTextfieldController>(textfield.get()));
    AddInstance(name, std::move(textfield));
  }

 private:
  std::vector<std::unique_ptr<SystemTextfieldController>> controllers_;
};

std::unique_ptr<SystemUIComponentsGridView>
CreateSystemTextfieldInstancesGridView() {
  auto grid_view = std::make_unique<TextfieldGridView>();

  // Small size textfield.
  auto textfield_small =
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kSmall);
  textfield_small->GetViewAccessibility().SetName(u"Small Text");
  textfield_small->SetPlaceholderText(u"Small Text");

  // Medium size textfield.
  auto textfield_medium =
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kMedium);
  textfield_medium->GetViewAccessibility().SetName(u"Medium Text");
  textfield_medium->SetPlaceholderText(u"Medium Text");

  // Large size textfield.
  auto textfield_large =
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kLarge);
  textfield_large->GetViewAccessibility().SetName(u"Large Text");
  textfield_large->SetPlaceholderText(u"Large Text");

  // Disabled textfield.
  auto textfield_disabled =
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kMedium);
  textfield_disabled->GetViewAccessibility().SetName(u"Disabled Text");
  textfield_disabled->SetPlaceholderText(u"Disable Text");
  textfield_disabled->SetEnabled(false);

  grid_view->AddTextfieldWithController(u"Textfield Small",
                                        std::move(textfield_small));
  grid_view->AddTextfieldWithController(u"Textfield Medium",
                                        std::move(textfield_medium));
  grid_view->AddTextfieldWithController(u"Textfield Large",
                                        std::move(textfield_large));
  grid_view->AddTextfieldWithController(u"Textfield Disabled",
                                        std::move(textfield_disabled));
  return grid_view;
}

}  // namespace ash
