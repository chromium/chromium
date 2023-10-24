// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_MOCK_DLP_WARN_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_MOCK_DLP_WARN_NOTIFIER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

using ::testing::Mock;

namespace policy {

// Allows tests to simulate the user's response to the warning dialog.
class MockDlpWarnNotifier : public DlpWarnNotifier {
 public:
  // Creates a mock object that propagates all calls to a real DlpWarnNotifier.
  MockDlpWarnNotifier();
  // Creates a mock object that can simulates user addressing the dialog, as
  // determined by value of |should_proceed|.
  explicit MockDlpWarnNotifier(bool should_proceed);
  MockDlpWarnNotifier(const MockDlpWarnNotifier& other) = delete;
  MockDlpWarnNotifier& operator=(const MockDlpWarnNotifier& other) = delete;
  ~MockDlpWarnNotifier() override;

  MOCK_METHOD(base::WeakPtr<views::Widget>,
              ShowDlpWarningDialog,
              (WarningCallback callback,
               DlpWarnDialog::DlpWarnDialogOptions options),
              (override));

 private:
  const bool should_proceed_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_MOCK_DLP_WARN_NOTIFIER_H_
