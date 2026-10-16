// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/selection/chrome_selection_dropdown_menu_delegate.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller_impl.h"
#include "chrome/browser/password_manager/factories/password_counter_factory.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/password_counter.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/menus/simple_menu_model.h"
#include "url/gurl.h"

namespace {

constexpr char kTestUrl[] = "https://example.com/";
constexpr char16_t kTestUsername[] = u"test_user@example.com";
constexpr char16_t kTestPassword[] = u"test_password";

class MockAutofillDriver : public autofill::TestContentAutofillDriver {
 public:
  using autofill::TestContentAutofillDriver::TestContentAutofillDriver;

  MOCK_METHOD(void,
              RendererShouldTriggerSuggestions,
              (const autofill::FieldGlobalId& field_id,
               autofill::AutofillSuggestionTriggerSource trigger_source),
              (override));
};

class MockPasswordAccessoryController : public PasswordAccessoryControllerImpl {
 public:
  MockPasswordAccessoryController(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* password_client)
      : PasswordAccessoryControllerImpl(
            web_contents,
            /*credential_cache=*/nullptr,
            /*manual_filling_controller=*/nullptr,
            password_client,
            /*driver_supplier=*/base::NullCallback(),
            /*grouped_credential_sheet_controller=*/nullptr,
            /*password_manager_error_message_helper_bridge=*/nullptr) {}

  MOCK_METHOD(void,
              OnOptionSelected,
              (autofill::AccessoryAction selected_action),
              (override));
};

class TestPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  const password_manager::PasswordManagerInterface* GetPasswordManager()
      const override {
    return &password_manager_;
  }

 private:
  password_manager::PasswordManager password_manager_{this};
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

content::ContextMenuParams CreateContextMenuParams(
    std::optional<blink::mojom::FormControlType> form_control_type =
        blink::mojom::FormControlType::kInputText,
    bool is_content_editable = false,
    const std::u16string& selection_text = std::u16string(),
    bool is_editable = true) {
  content::ContextMenuParams params;
  params.selection_text = selection_text;
  params.is_editable = is_editable;
  params.is_content_editable_for_autofill = is_content_editable;
  params.page_url = GURL(kTestUrl);
  params.frame_url = GURL(kTestUrl);
  params.form_control_type =
      (!is_editable || is_content_editable) ? std::nullopt : form_control_type;
  return params;
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
    password_manager::ContentPasswordManagerDriverFactory::CreateForWebContents(
        web_contents(), &password_manager_client_);
    NavigateAndCommit(GURL(kTestUrl));
  }

 protected:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            ProfilePasswordStoreFactory::GetInstance(),
            base::BindRepeating(&password_manager::BuildPasswordStore<
                                content::BrowserContext,
                                password_manager::TestPasswordStore>)},
        TestingProfile::TestingFactory{
            PasswordCounterFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              Profile* profile = Profile::FromBrowserContext(context);
              return std::make_unique<password_manager::PasswordCounter>(
                  ProfilePasswordStoreFactory::GetForProfile(
                      profile, ServiceAccessType::EXPLICIT_ACCESS)
                      .get(),
                  /*account_store=*/nullptr);
            })},
    };
  }

  MockAutofillDriver* autofill_driver() {
    return autofill_driver_injector_[main_rfh()];
  }
  autofill::TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }
  password_manager::PasswordManagerClient* password_client() {
    return &password_manager_client_;
  }

  [[nodiscard]] testing::AssertionResult AddSavedPassword() {
    password_manager::StoredCredential cred;
    cred.url = GURL(kTestUrl);
    cred.signon_realm = cred.url.GetWithEmptyPath().spec();
    cred.username_value = kTestUsername;
    cred.password_value = password_manager::PasswordString(kTestPassword);
    ProfilePasswordStoreFactory::GetForProfile(
        profile(), ServiceAccessType::EXPLICIT_ACCESS)
        ->AddLogin(std::move(cred));
    password_manager::PasswordCounter* counter =
        PasswordCounterFactory::GetForProfile(profile());
    return base::test::RunUntil(
               [&] { return counter->autofillable_passwords() > 0; })
               ? testing::AssertionSuccess()
               : testing::AssertionFailure()
                     << "Timed out waiting for saved password.";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillDriverInjector<testing::NiceMock<MockAutofillDriver>>
      autofill_driver_injector_;
  TestPasswordManagerClient password_manager_client_;
};

#if BUILDFLAG(ENABLE_PRINTING)
TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_PrintEnabledWithSelection) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params = CreateContextMenuParams(
      /*form_control_type=*/std::nullopt, /*is_content_editable=*/false,
      /*selection_text=*/u"hello", /*is_editable=*/false);

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
  content::ContextMenuParams params = CreateContextMenuParams(
      /*form_control_type=*/std::nullopt, /*is_content_editable=*/false,
      /*selection_text=*/u"", /*is_editable=*/false);

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
  content::ContextMenuParams params = CreateContextMenuParams(
      blink::mojom::FormControlType::kInputPassword,
      /*is_content_editable=*/false, /*selection_text=*/u"password123");

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
  content::ContextMenuParams params = CreateContextMenuParams(
      /*form_control_type=*/std::nullopt, /*is_content_editable=*/false,
      /*selection_text=*/u"hello", /*is_editable=*/false);

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
  content::ContextMenuParams params = CreateContextMenuParams(
      blink::mojom::FormControlType::kInputText, /*is_content_editable=*/false,
      /*selection_text=*/u"hello");

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
  content::ContextMenuParams params = CreateContextMenuParams(
      /*form_control_type=*/std::nullopt, /*is_content_editable=*/false,
      /*selection_text=*/u"hello", /*is_editable=*/false);

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
  content::ContextMenuParams params = CreateContextMenuParams();

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
  content::ContextMenuParams params = CreateContextMenuParams(
      /*form_control_type=*/std::nullopt, /*is_content_editable=*/true);

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
  content::ContextMenuParams params =
      CreateContextMenuParams(blink::mojom::FormControlType::kInputPassword);

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  EXPECT_FALSE(
      ContainsCommand(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_AtMemoryNonEditable) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params = CreateContextMenuParams(
      /*form_control_type=*/std::nullopt, /*is_content_editable=*/false,
      /*selection_text=*/u"hello", /*is_editable=*/false);

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  EXPECT_FALSE(
      ContainsCommand(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_AtMemoryExecuteCommand) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params = CreateContextMenuParams();
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

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       OnShowSelectionPopup_HideAutofillSuggestions) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;

  params.source_type = ui::mojom::MenuSourceType::kTouch;
  delegate.OnShowSelectionPopup(*main_rfh(), params);
  EXPECT_NE(autofill_client()->popup_hiding_reason(),
            autofill::SuggestionHidingReason::kContextMenuOpened);

  params.source_type = ui::mojom::MenuSourceType::kMouse;
  delegate.OnShowSelectionPopup(*main_rfh(), params);
  EXPECT_EQ(autofill_client()->popup_hiding_reason(),
            autofill::SuggestionHidingReason::kContextMenuOpened);
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       OnShowSelectionPopup_HideAutofillSuggestionsFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      autofill::features::kAutofillAndroidHideSuggestionsOnContextMenu);

  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.source_type = ui::mojom::MenuSourceType::kMouse;

  delegate.OnShowSelectionPopup(*main_rfh(), params);
  EXPECT_NE(autofill_client()->popup_hiding_reason(),
            autofill::SuggestionHidingReason::kContextMenuOpened);
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_SelectPasswordPasswordField) {
  ASSERT_TRUE(AddSavedPassword());
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params =
      CreateContextMenuParams(blink::mojom::FormControlType::kInputPassword);

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  EXPECT_FALSE(
      ContainsCommand(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY));
  std::optional<size_t> select_password_index = FindCommandIndex(
      *model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD);
  ASSERT_TRUE(select_password_index.has_value());
  EXPECT_TRUE(model->IsEnabledAt(*select_password_index));
  EXPECT_EQ(6, model->GetDisplayOrderAt(*select_password_index));

  // Verify the trailing separator immediately after Select Password also has
  // order 6.
  ASSERT_LT(*select_password_index + 1, model->GetItemCount());
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
            model->GetTypeAt(*select_password_index + 1));
  EXPECT_EQ(6, model->GetDisplayOrderAt(*select_password_index + 1));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_SelectPasswordAndAtMemoryEditableField) {
  ASSERT_TRUE(AddSavedPassword());
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params = CreateContextMenuParams();

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  std::optional<size_t> at_memory_index =
      FindCommandIndex(*model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_AT_MEMORY);
  std::optional<size_t> select_password_index = FindCommandIndex(
      *model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD);
  ASSERT_TRUE(at_memory_index.has_value());
  ASSERT_TRUE(select_password_index.has_value());
  EXPECT_EQ(5, model->GetDisplayOrderAt(*at_memory_index));
  EXPECT_EQ(6, model->GetDisplayOrderAt(*select_password_index));

  // Verify the trailing separator after both items uses the preceding item's
  // order (6).
  ASSERT_LT(*select_password_index + 1, model->GetItemCount());
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
            model->GetTypeAt(*select_password_index + 1));
  EXPECT_EQ(6, model->GetDisplayOrderAt(*select_password_index + 1));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_SelectPasswordNoSavedPasswords) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params =
      CreateContextMenuParams(blink::mojom::FormControlType::kInputPassword);

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  EXPECT_FALSE(ContainsCommand(
      *model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_SelectPasswordTextArea) {
  ASSERT_TRUE(AddSavedPassword());
  ChromeSelectionDropdownMenuDelegate delegate;

  content::ContextMenuParams params =
      CreateContextMenuParams(blink::mojom::FormControlType::kTextArea);
  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);
  ASSERT_TRUE(model);
  EXPECT_FALSE(ContainsCommand(
      *model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_SelectPasswordContentEditable) {
  ASSERT_TRUE(AddSavedPassword());
  ChromeSelectionDropdownMenuDelegate delegate;

  content::ContextMenuParams params = CreateContextMenuParams(
      /*form_control_type=*/std::nullopt, /*is_content_editable=*/true);
  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);
  ASSERT_TRUE(model);
  EXPECT_FALSE(ContainsCommand(
      *model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD));
}

TEST_F(ChromeSelectionDropdownMenuDelegateTest,
       GetSelectionPopupExtraItems_SelectPasswordExecuteCommand) {
  ASSERT_TRUE(AddSavedPassword());
  auto mock_accessory = std::make_unique<MockPasswordAccessoryController>(
      web_contents(), password_client());
  MockPasswordAccessoryController* mock_accessory_ptr = mock_accessory.get();
  web_contents()->SetUserData(PasswordAccessoryControllerImpl::UserDataKey(),
                              std::move(mock_accessory));

  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params =
      CreateContextMenuParams(blink::mojom::FormControlType::kInputPassword);

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  std::optional<size_t> select_password_index = FindCommandIndex(
      *model, IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD);
  ASSERT_TRUE(select_password_index.has_value());

  EXPECT_CALL(*mock_accessory_ptr,
              OnOptionSelected(autofill::AccessoryAction::USE_OTHER_PASSWORD));

  model->ActivatedAt(*select_password_index);
}

}  // namespace android
