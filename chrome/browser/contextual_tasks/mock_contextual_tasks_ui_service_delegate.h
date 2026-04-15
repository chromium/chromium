// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_H_

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

class MockContextualTasksUiServiceDelegate
    : public ContextualTasksUiServiceDelegate {
 public:
  MockContextualTasksUiServiceDelegate();
  ~MockContextualTasksUiServiceDelegate() override;

  MOCK_METHOD(void,
              OpenFeedbackUi,
              (BrowserWindowInterface * browser, const GURL& page_url),
              (override));
  MOCK_METHOD(void,
              ShowUndoSnackbar,
              (BrowserWindowInterface * browser_window_interface),
              (override));
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_H_
