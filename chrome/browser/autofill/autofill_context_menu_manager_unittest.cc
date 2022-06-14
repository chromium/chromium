// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_context_menu_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

class AutofillContextMenuManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillContextMenuManagerTest() {
    feature_.InitAndEnableFeature(
        features::kAutofillShowManualFallbackInContextMenu);
  }

  AutofillContextMenuManagerTest(const AutofillContextMenuManagerTest&) =
      delete;
  AutofillContextMenuManagerTest& operator=(
      const AutofillContextMenuManagerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        profile(), BrowserContextKeyedServiceFactory::TestingFactory());

    personal_data_manager_ = std::make_unique<TestPersonalDataManager>();
    personal_data_manager_->SetPrefService(profile()->GetPrefs());
    menu_model_ = std::make_unique<ui::SimpleMenuModel>(nullptr);

    personal_data_manager_->AddProfile(test::GetFullProfile());
    personal_data_manager_->AddCreditCard(test::GetCreditCard());

    autofill_context_menu_manager_ =
        std::make_unique<AutofillContextMenuManager>(
            personal_data_manager_.get(), nullptr, menu_model_.get());
  }

  void TearDown() override {
    personal_data_manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  ui::SimpleMenuModel* menu_model() const { return menu_model_.get(); }

  AutofillContextMenuManager* autofill_context_menu_manager() const {
    return autofill_context_menu_manager_.get();
  }

 private:
  std::unique_ptr<TestPersonalDataManager> personal_data_manager_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<AutofillContextMenuManager> autofill_context_menu_manager_;
  base::test::ScopedFeatureList feature_;
};

// Tests that the Autofill context menu is correctly set up.
TEST_F(AutofillContextMenuManagerTest, AutofillContextMenuContents) {
  autofill_context_menu_manager()->AppendItems();

  std::vector<std::u16string> all_added_strings;

  // Check for top level menu with autofill options.
  ASSERT_EQ(2, menu_model()->GetItemCount());
  ASSERT_EQ(u"Fill Address Info", menu_model()->GetLabelAt(0));
  ASSERT_EQ(u"Fill Payment", menu_model()->GetLabelAt(1));
  ASSERT_EQ(menu_model()->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_SUBMENU);
  ASSERT_EQ(menu_model()->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SUBMENU);

  // Check for submenu with address descriptions.
  auto* address_menu_model = menu_model()->GetSubmenuModelAt(0);
  ASSERT_EQ(address_menu_model->GetItemCount(), 1);
  ASSERT_EQ(u"John H. Doe, 666 Erebus St.", address_menu_model->GetLabelAt(0));
  ASSERT_EQ(address_menu_model->GetTypeAt(0),
            ui::MenuModel::ItemType::TYPE_SUBMENU);

  // Check for submenu with address details.
  auto* address_details_submenu = address_menu_model->GetSubmenuModelAt(0);
  ASSERT_EQ(address_details_submenu->GetItemCount(), 6);
  static constexpr std::array expected_address_values = {
      u"666 Erebus St.\nApt 8", u"Elysium",          u"91111", u"",
      u"16502111111",           u"johndoe@hades.com"};
  for (size_t i = 0; i < expected_address_values.size(); i++) {
    ASSERT_EQ(address_details_submenu->GetLabelAt(i),
              expected_address_values[i]);
    all_added_strings.push_back(expected_address_values[i]);
  }

  // Check for submenu with credit card descriptions.
  auto* card_menu_model = menu_model()->GetSubmenuModelAt(1);
  ASSERT_EQ(card_menu_model->GetItemCount(), 1);
  ASSERT_EQ(
      u"Visa  "
      u"\x202A\x2022\x2060\x2006\x2060\x2022\x2060\x2006\x2060\x2022\x2060"
      u"\x2006\x2060\x2022\x2060\x2006\x2060"
      u"1111\x202C",
      card_menu_model->GetLabelAt(0));
  ASSERT_EQ(card_menu_model->GetTypeAt(0),
            ui::MenuModel::ItemType::TYPE_SUBMENU);

  // Check for submenu with credit card details.
  auto* card_details_submenu = card_menu_model->GetSubmenuModelAt(0);
  ASSERT_EQ(card_details_submenu->GetItemCount(), 5);
  static constexpr std::array expected_credit_card_values = {
      u"Test User", u"4111111111111111", u""};
  for (size_t i = 0; i < expected_credit_card_values.size(); i++) {
    ASSERT_EQ(card_details_submenu->GetLabelAt(i),
              expected_credit_card_values[i]);
    all_added_strings.push_back(expected_credit_card_values[i]);
  }
  all_added_strings.push_back(base::ASCIIToUTF16(test::NextMonth().c_str()));
  ASSERT_EQ(card_details_submenu->GetLabelAt(3), all_added_strings.back());
  all_added_strings.push_back(
      base::ASCIIToUTF16(test::NextYear().c_str()).substr(2));
  ASSERT_EQ(card_details_submenu->GetLabelAt(4), all_added_strings.back());

  // Test all strings added to the command_id_to_menu_item_value_mapper were
  // added to the context menu.
  auto mapper = autofill_context_menu_manager()
                    ->command_id_to_menu_item_value_mapper_for_testing();
  base::ranges::sort(all_added_strings);
  EXPECT_TRUE(base::ranges::all_of(mapper, [&](const auto& p) {
    return base::Contains(all_added_strings, p.second.fill_value);
  }));
}

}  // namespace autofill
