// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_UI_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_UI_SERVICE_H_

#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

class MockContextualTasksUiService : public ContextualTasksUiService {
 public:
  MockContextualTasksUiService(
      Profile* profile,
      ContextualTasksService* service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service,
      std::unique_ptr<ContextualTasksEligibilityManager> eligibility_manager,
      std::unique_ptr<ContextualTasksCookieSynchronizer> cookie_synchronizer);
  ~MockContextualTasksUiService() override;

  MOCK_METHOD(void,
              OnTaskChanged,
              (BrowserWindowInterface * browser_window_interface,
               content::WebContents* web_contents,
               const std::optional<base::Uuid>& old_task_id,
               const std::optional<base::Uuid>& new_task_id,
               bool is_shown_in_tab),
              (override));
  MOCK_METHOD(void,
              OnWebUIReady,
              (BrowserWindowInterface * browser_window_interface,
               const base::Uuid& task_id,
               content::WebContents* web_contents),
              (override));
  MOCK_METHOD(void,
              OnWebUIDestroyed,
              (BrowserWindowInterface * browser_window_interface,
               const std::optional<base::Uuid>& task_id),
              (override));
  MOCK_METHOD(GURL, GetDefaultAiPageUrl, (), (override));
  MOCK_METHOD(GURL,
              GetDefaultAiPageUrlForTask,
              (const base::Uuid& task_id),
              (override));
  MOCK_METHOD(void,
              SetInitialEntryPointForTask,
              (const base::Uuid&, omnibox::ChromeAimEntryPoint),
              (override));
  MOCK_METHOD(std::optional<GURL>,
              GetInitialUrlForTask,
              (const base::Uuid&),
              (override));
  MOCK_METHOD(std::optional<GURL>,
              GetCreationUrlForTask,
              (const base::Uuid&),
              (override));
  MOCK_METHOD(void,
              GetThreadUrlFromTaskId,
              (const base::Uuid&, base::OnceCallback<void(GURL)>),
              (override));
  MOCK_METHOD(void,
              MoveTaskUiToNewTab,
              (const base::Uuid&, BrowserWindowInterface*, const GURL&),
              (override));
  MOCK_METHOD(void,
              OnTabClickedFromSourcesMenu,
              (int32_t, const GURL&, BrowserWindowInterface*),
              (override));
  MOCK_METHOD(void,
              OnFileClickedFromSourcesMenu,
              (const GURL&, BrowserWindowInterface*),
              (override));
  MOCK_METHOD(void,
              OnImageClickedFromSourcesMenu,
              (const GURL&, BrowserWindowInterface*),
              (override));
  MOCK_METHOD(bool, IsAiUrl, (const GURL&), (override));
  MOCK_METHOD(bool, IsUrlForPrimaryAccount, (const GURL&), (override));
  MOCK_METHOD(bool, IsPendingErrorPage, (const base::Uuid&), (override));
  MOCK_METHOD(void,
              OpenFeedbackUi,
              (BrowserWindowInterface*, const GURL&),
              (override));
  MOCK_METHOD(ContextualTasksEligibilityManager*,
              GetEligibilityManager,
              (),
              (const, override));
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_UI_SERVICE_H_
