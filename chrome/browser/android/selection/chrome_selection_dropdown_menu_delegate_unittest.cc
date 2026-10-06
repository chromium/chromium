// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/selection/chrome_selection_dropdown_menu_delegate.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

namespace {

class MockAutofillDriver : public autofill::TestContentAutofillDriver {
 public:
  using autofill::TestContentAutofillDriver::TestContentAutofillDriver;

  MOCK_METHOD(void,
              RendererShouldTriggerSuggestions,
              (const autofill::FieldGlobalId& field_id,
               autofill::AutofillSuggestionTriggerSource trigger_source),
              (override));
};

std::optional<size_t> FindCommandIndex(const ui::MenuModel& model,
                                       int command_id) {
  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    if (model.GetCommandIdAt(i) == command_id) {
      return i;
    }
  }
  return std::nullopt;
}

bool ContainsCommand(const ui::MenuModel& model, int command_id) {
  return FindCommandIndex(model, command_id).has_value();
}

bool IsCommandEnabled(const ui::MenuModel& model, int command_id) {
  if (std::optional<size_t> index = FindCommandIndex(model, command_id)) {
    return model.IsEnabledAt(*index);
  }
  return false;
}

int GetCommandOrder(const ui::MenuModel& model, int command_id) {
  if (std::optional<size_t> index = FindCommandIndex(model, command_id)) {
    return model.GetDisplayOrderAt(*index);
  }
  return -1;
}

}  // namespace

namespace android {

class ChromeSelectionDropdownMenuDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeSelectionDropdownMenuDelegateTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {chrome::android::kPrintSelectionMenu,
         features::kAndroidDevToolsFrontend,
         autofill::features::kAutofillAtMemory,
         autofill::features::debug::kAtMemorySkipEnablementChecks},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com"));
  }

 protected:
  MockAutofillDriver* autofill_driver() {
    return autofill_driver_injector_[main_rfh()];
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillDriverInjector<testing::NiceMock<MockAutofillDriver>>
      autofill_driver_injector_;
};

#if BUILDFLAG(ENABLE_PRINTING)
TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_PrintEnabledWithSelection) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.selection_text = u"hello";

  // Enable printing pref.
  profile()->GetPrefs()->SetBoolean(prefs::kPrintingEnabled, true);

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  ASSERT_TRUE(ContainsCommand(*model, IDC_PRINT));
  EXPECT_TRUE(IsCommandEnabled(*model, IDC_PRINT));
  EXPECT_EQ(65, GetCommandOrder(*model, IDC_PRINT));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_PrintEnabledNoSelection) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  // params.selection_text is empty.

  // Enable printing pref.
  profile()->GetPrefs()->SetBoolean(prefs::kPrintingEnabled, true);

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  ASSERT_TRUE(ContainsCommand(*model, IDC_PRINT));
  EXPECT_FALSE(IsCommandEnabled(*model, IDC_PRINT));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_PrintEnabledPassword) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.selection_text = u"password123";
  params.form_control_type = blink::mojom::FormControlType::kInputPassword;

  // Enable printing pref.
  profile()->GetPrefs()->SetBoolean(prefs::kPrintingEnabled, true);

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  ASSERT_TRUE(ContainsCommand(*model, IDC_PRINT));
  EXPECT_FALSE(IsCommandEnabled(*model, IDC_PRINT));
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_PrintDisabled) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.selection_text = u"hello";

#if BUILDFLAG(ENABLE_PRINTING)
  // Disable printing pref.
  profile()->GetPrefs()->SetBoolean(prefs::kPrintingEnabled, false);
#endif

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
#if BUILDFLAG(ENABLE_PRINTING)
  ASSERT_TRUE(ContainsCommand(*model, IDC_PRINT));
  EXPECT_FALSE(IsCommandEnabled(*model, IDC_PRINT));
#else
  EXPECT_FALSE(ContainsCommand(*model, IDC_PRINT));
#endif
}

class ChromeSelectionDropdownMenuDelegateFeatureDisabledTest
    : public ChromeSelectionDropdownMenuDelegateTest {
 public:
  ChromeSelectionDropdownMenuDelegateFeatureDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            chrome::android::kPrintSelectionMenu,
            features::kAndroidDevToolsFrontend,
            autofill::features::kAutofillAtMemory,
            autofill::features::debug::kAtMemorySkipEnablementChecks});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeSelectionDropdownMenuDelegateFeatureDisabledTest,
       GetSelectionPopupExtraItems_FeatureDisabled) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.selection_text = u"hello";
  params.is_editable = true;
  params.page_url = GURL("https://example.com");
  params.frame_url = GURL("https://example.com");
  params.form_control_type = blink::mojom::FormControlType::kInputText;

#if BUILDFLAG(ENABLE_PRINTING)
  profile()->GetPrefs()->SetBoolean(prefs::kPrintingEnabled, true);
#endif

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  EXPECT_FALSE(ContainsCommand(*model, IDC_PRINT));
  EXPECT_FALSE(ContainsCommand(*model, IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  EXPECT_FALSE(
      ContainsCommand(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_InspectOrder) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.selection_text = u"hello";

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  ASSERT_TRUE(ContainsCommand(*model, IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  EXPECT_TRUE(IsCommandEnabled(*model, IDC_CONTENT_CONTEXT_INSPECTELEMENT));
  EXPECT_EQ(1000000,
            GetCommandOrder(*model, IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_AtMemoryEditableField) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.is_editable = true;
  params.page_url = GURL("https://example.com");
  params.frame_url = GURL("https://example.com");
  params.form_control_type = blink::mojom::FormControlType::kInputText;

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  std::optional<size_t> at_memory_index =
      FindCommandIndex(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY);
  ASSERT_TRUE(at_memory_index.has_value());
  EXPECT_TRUE(model->IsEnabledAt(*at_memory_index));
  EXPECT_EQ(5, model->GetDisplayOrderAt(*at_memory_index));

  // Verify the trailing separator immediately after @memory also has order 5.
  ASSERT_LT(*at_memory_index + 1, model->GetItemCount());
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
            model->GetTypeAt(*at_memory_index + 1));
  EXPECT_EQ(5, model->GetDisplayOrderAt(*at_memory_index + 1));

  // Verify Inspect Element and its preceding separator still have order
  // 1000000.
  std::optional<size_t> inspect_index =
      FindCommandIndex(*model, IDC_CONTENT_CONTEXT_INSPECTELEMENT);
  ASSERT_TRUE(inspect_index.has_value());
  EXPECT_EQ(1000000, model->GetDisplayOrderAt(*inspect_index));
  ASSERT_GT(*inspect_index, 0u);
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
            model->GetTypeAt(*inspect_index - 1));
  EXPECT_EQ(1000000, model->GetDisplayOrderAt(*inspect_index - 1));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_AtMemoryContentEditable) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.is_editable = true;
  params.is_content_editable_for_autofill = true;
  params.page_url = GURL("https://example.com");
  params.frame_url = GURL("https://example.com");

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  EXPECT_TRUE(
      ContainsCommand(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY));
  EXPECT_TRUE(IsCommandEnabled(
      *model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY));
  EXPECT_EQ(5, GetCommandOrder(
                   *model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_AtMemoryPasswordField) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.is_editable = true;
  params.page_url = GURL("https://example.com");
  params.frame_url = GURL("https://example.com");
  params.form_control_type = blink::mojom::FormControlType::kInputPassword;

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  EXPECT_FALSE(
      ContainsCommand(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_AtMemoryNonEditable) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.selection_text = u"hello";
  params.page_url = GURL("https://example.com");
  params.frame_url = GURL("https://example.com");

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  EXPECT_FALSE(
      ContainsCommand(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_AtMemoryExecuteCommand) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.is_editable = true;
  params.page_url = GURL("https://example.com");
  params.frame_url = GURL("https://example.com");
  params.form_control_type = blink::mojom::FormControlType::kInputText;
  params.field_renderer_id = blink::DOMNodeIdType(42);

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  std::optional<size_t> at_memory_index =
      FindCommandIndex(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY);
  ASSERT_TRUE(at_memory_index.has_value());

  EXPECT_CALL(
      *autofill_driver(),
      RendererShouldTriggerSuggestions(
          autofill::FieldGlobalId{autofill_driver()->GetFrameToken(),
                                  autofill::FieldRendererId(42)},
          autofill::AutofillSuggestionTriggerSource::kAtMemoryContextMenu));

  model->ActivatedAt(*at_memory_index);
}

}  // namespace android
