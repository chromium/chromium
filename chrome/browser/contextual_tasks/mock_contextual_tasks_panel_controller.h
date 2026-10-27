// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_PANEL_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

namespace content {
class WebContents;
}

namespace contextual_tasks {

class MockContextualTasksPanelController
    : public ContextualTasksPanelController {
 public:
  MockContextualTasksPanelController();
  explicit MockContextualTasksPanelController(
      BrowserWindowInterface* browser_window);
  ~MockContextualTasksPanelController() override;

  MOCK_METHOD(void,
              Show,
              (bool,
               omnibox::ChromeAimEntryPoint,
               bool,
               std::optional<base::TimeTicks>,
               std::optional<EntrySource>),
              (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, OpenInZeroState, (), (override));
  MOCK_METHOD(bool, MoveActiveTabToNewThreadIfShared, (), (override));
  MOCK_METHOD((std::map<std::string, std::string>),
              GetTabRestoreData,
              (tabs::TabInterface*),
              (const, override));
  MOCK_METHOD(void,
              ShareTabsWithActiveOrNewThread,
              (const std::vector<tabs::TabHandle>&),
              (override));
  MOCK_METHOD(void,
              ShareTabsWithExistingThread,
              (const base::Uuid&,
               const std::vector<tabs::TabHandle>&,
               omnibox::ChromeAimEntryPoint),
              (override));
  MOCK_METHOD(std::vector<RecentThreadInfo>,
              GetRecentThreads,
              (),
              (const, override));
  MOCK_METHOD(void,
              UnshareTabs,
              (const std::vector<tabs::TabHandle>&),
              (override));
  MOCK_METHOD(bool, IsPanelOpenForContextualTask, (), (const, override));
  MOCK_METHOD(bool, IsShowingResults, (), (const, override));
  MOCK_METHOD(bool, HasResults, (), (const, override));
  MOCK_METHOD(ContextualTasksPanelController::EntrySource,
              GetActiveEntrySource,
              (),
              (const, override));
  MOCK_METHOD(bool, IsOnboardingTooltipVisible, (), (const, override));
  MOCK_METHOD(std::optional<tabs::TabHandle>,
              GetAutoSuggestedTabHandle,
              (),
              (override));
  MOCK_METHOD(void,
              OnTaskChanged,
              (content::WebContents*, base::Uuid),
              (override));
  MOCK_METHOD(void, OnAiInteraction, (), (override));
  MOCK_METHOD(void,
              SetPendingTaskForTab,
              (tabs::TabInterface*, const base::Uuid&),
              (override));
  MOCK_METHOD(content::WebContents*,
              GetActiveWebContents,
              (),
              (const override));
  MOCK_METHOD(content::WebContents*,
              GetToolbarWebContents,
              (),
              (const override));
  MOCK_METHOD(std::vector<content::WebContents*>,
              GetPanelWebContentsList,
              (),
              (const, override));
  MOCK_METHOD(void, CleanUpUnusedWebContents, (), (override));
  MOCK_METHOD(std::unique_ptr<content::WebContents>,
              DetachWebContentsForTask,
              (const base::Uuid&),
              (override));
  MOCK_METHOD(contextual_search::ContextualSearchSessionHandle*,
              GetContextualSearchSessionHandleForPanel,
              (),
              (override));
  MOCK_METHOD(void,
              TransferWebContentsFromTab,
              (const base::Uuid&, std::unique_ptr<content::WebContents>),
              (override));
  MOCK_METHOD(std::optional<ContextualTask>, GetCurrentTask, (), (override));
  MOCK_METHOD((std::pair<std::optional<base::Uuid>,
                         contextual_search::ContextualSearchSessionHandle*>),
              GetSessionHandleForActiveTabOrPanel,
              (),
              (override));
  MOCK_METHOD(size_t, GetNumberOfActiveTasks, (), (const, override));
  MOCK_METHOD(void, MoveTaskUiToNewTab, (), (override));
  MOCK_METHOD(void, NotifyExpandToFullTabStateChanged, (), (override));
  MOCK_METHOD(bool, CanExpandToFullTab, (), (const, override));
  MOCK_METHOD(void,
              ShowPageInfoBubble,
              (bool is_pointer_interaction),
              (override));
  MOCK_METHOD(void, OnLogoPointerDown, (), (override));
  MOCK_METHOD(ContextualTasksPermissionController*,
              permission_controller,
              (),
              (override));
  MOCK_METHOD(void,
              RecordTimeToHandshakeComplete,
              (content::WebContents*),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));

 private:
  std::optional<ui::ScopedUnownedUserData<ContextualTasksPanelController>>
      scoped_unowned_user_data_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_PANEL_CONTROLLER_H_
