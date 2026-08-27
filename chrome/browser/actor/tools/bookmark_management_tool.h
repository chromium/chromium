// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_BOOKMARK_MANAGEMENT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_BOOKMARK_MANAGEMENT_TOOL_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/tool.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

namespace actor {

// Tool for managing bookmarks in the user's profile.
// - `Action::kAdd`: Adds a bookmark for the specified URL and title under the
//   "Other Bookmarks" folder.
// - `Action::kRemove`: Removes all bookmarks matching the specified URL across
//   all bookmark folders.
class BookmarkManagementTool : public Tool {
 public:
  enum class Action {
    kAdd,
    kRemove,
  };

  BookmarkManagementTool(TaskId task_id,
                         ToolDelegate& tool_delegate,
                         Action action,
                         GURL url,
                         std::u16string title = u"");
  ~BookmarkManagementTool() override;

  // Tool:
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  const Action action_;
  const GURL url_;
  const std::u16string title_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_BOOKMARK_MANAGEMENT_TOOL_H_
