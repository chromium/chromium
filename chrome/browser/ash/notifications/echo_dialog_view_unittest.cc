// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/echo_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/notifications/echo_dialog_listener.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/views_test_base.h"

namespace ash {

namespace {

using EchoDialogViewTest = ::views::ViewsTestBase;

class TestEchoDialogListener : public EchoDialogListener {
 public:
  TestEchoDialogListener() = default;
  ~TestEchoDialogListener() override = default;

 private:
  void OnAccept() override {}
  void OnCancel() override {}
  void OnMoreInfoLinkClicked() override {}

  TestEchoDialogListener(const TestEchoDialogListener&) = delete;
  TestEchoDialogListener& operator=(const TestEchoDialogListener&) = delete;
};

bool IsLabelWithText(const views::View* view, const std::u16string& text) {
  const char* class_name = view->GetClassName();
  if (!strcmp(class_name, "Label")) {
    auto* label = static_cast<const views::Label*>(view);
    return label->GetText().find(text) != label->GetText().npos;
  }
  if (!strcmp(class_name, "StyledLabel")) {
    auto* styled_label = static_cast<const views::StyledLabel*>(view);
    return styled_label->GetText().find(text) != styled_label->GetText().npos;
  }
  return false;
}

views::View* FindLabelWithText(views::View* root, const std::u16string& text) {
  if (IsLabelWithText(root, text))
    return root;
  for (views::View* child : root->children()) {
    views::View* matched = FindLabelWithText(child, text);
    if (matched)
      return matched;
  }
  return nullptr;
}

}  // namespace

// These two tests ensure that the dialog contains certain strings somewhere in
// its body depending on the params given to it.
TEST_F(EchoDialogViewTest, EnabledHasEnabledText) {
  EchoDialogView::Params params;
  params.echo_enabled = true;
  params.service_name = u"$service";
  params.origin = u"$origin";

  TestEchoDialogListener listener;
  EchoDialogView dialog(&listener, params);

  EXPECT_TRUE(FindLabelWithText(
      &dialog, l10n_util::GetStringFUTF16(IDS_ECHO_CONSENT_DIALOG_TEXT,
                                          params.service_name)));

  // The origin is not mentioned in the body text of the dialog, only in a
  // tooltip. Expect not to find it in body text.
  EXPECT_FALSE(FindLabelWithText(&dialog, params.origin));
}

TEST_F(EchoDialogViewTest, DisabledHasDisabledText) {
  EchoDialogView::Params params;
  params.echo_enabled = false;
  params.service_name = u"$service";
  params.origin = u"$origin";

  TestEchoDialogListener listener;
  EchoDialogView dialog(&listener, params);

  EXPECT_FALSE(FindLabelWithText(&dialog, params.service_name));
  EXPECT_FALSE(FindLabelWithText(&dialog, params.origin));
  EXPECT_TRUE(FindLabelWithText(
      &dialog,
      l10n_util::GetStringUTF16(IDS_ECHO_DISABLED_CONSENT_DIALOG_TEXT)));
}

}  // namespace ash
