// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/selection/chrome_selection_dropdown_menu_delegate.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"
#include "ui/menus/simple_menu_model.h"

namespace {

bool ContainsCommand(const ui::MenuModel& model, int command_id) {
  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    if (model.GetCommandIdAt(i) == command_id) {
      return true;
    }
  }
  return false;
}

int GetCommandOrder(const ui::MenuModel& model, int command_id) {
  for (size_t i = 0; i < model.GetItemCount(); ++i) {
    if (model.GetCommandIdAt(i) == command_id) {
      return model.GetDisplayOrderAt(i);
    }
  }
  return -1;
}

}  // namespace

namespace android {

class ChromeSelectionDropdownMenuDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeSelectionDropdownMenuDelegateTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chrome::android::kPrintSelectionMenu);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  EXPECT_FALSE(ContainsCommand(*model, IDC_PRINT));
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
  EXPECT_FALSE(ContainsCommand(*model, IDC_PRINT));
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
  EXPECT_FALSE(ContainsCommand(*model, IDC_PRINT));
}

class ChromeSelectionDropdownMenuDelegateFeatureDisabledTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeSelectionDropdownMenuDelegateFeatureDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        chrome::android::kPrintSelectionMenu);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeSelectionDropdownMenuDelegateFeatureDisabledTest,
       GetSelectionPopupExtraItems_FeatureDisabled) {
  ChromeSelectionDropdownMenuDelegate delegate;
  content::ContextMenuParams params;
  params.selection_text = u"hello";

#if BUILDFLAG(ENABLE_PRINTING)
  profile()->GetPrefs()->SetBoolean(prefs::kPrintingEnabled, true);
#endif

  std::unique_ptr<ui::MenuModel> model =
      delegate.GetSelectionPopupExtraItems(*main_rfh(), params);

  ASSERT_TRUE(model);
  EXPECT_FALSE(ContainsCommand(*model, IDC_PRINT));
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
  EXPECT_EQ(1000000,
            GetCommandOrder(*model, IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

}  // namespace android
