// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/bookmark_management_tool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"

#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

namespace {

bookmarks::BookmarkModel* GetBookmarkModel(ToolDelegate& tool_delegate) {
  Profile& profile = tool_delegate.GetProfile();
  return BookmarkModelFactory::GetForBrowserContext(&profile);
}

}  // namespace

BookmarkManagementTool::BookmarkManagementTool(TaskId task_id,
                                               ToolDelegate& tool_delegate,
                                               Action action,
                                               GURL url,
                                               std::u16string title)
    : Tool(task_id, tool_delegate),
      action_(action),
      url_(std::move(url)),
      title_(std::move(title)) {}

BookmarkManagementTool::~BookmarkManagementTool() = default;

void BookmarkManagementTool::Validate(ToolCallback callback) {
  if (url_.is_empty() || !url_.is_valid()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                   /*requires_page_stabilization=*/false,
                   "The provided bookmark URL is empty or invalid."));
    return;
  }

  bookmarks::BookmarkModel* bookmark_model = GetBookmarkModel(tool_delegate());
  if (!bookmark_model || !bookmark_model->loaded()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kBookmarkModelNotLoaded,
                   /*requires_page_stabilization=*/false,
                   "BookmarkModel is not loaded or not available."));
    return;
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

void BookmarkManagementTool::Invoke(ToolCallback callback) {
  bookmarks::BookmarkModel* bookmark_model = GetBookmarkModel(tool_delegate());
  CHECK(bookmark_model);
  CHECK(bookmark_model->loaded());

  switch (action_) {
    case Action::kAdd: {
      const bookmarks::BookmarkNode* parent = bookmark_model->other_node();
      CHECK(parent);
      bookmark_model->AddNewURL(parent, parent->children().size(), title_,
                                url_);
      break;
    }
    case Action::kRemove: {
      for (const bookmarks::BookmarkNode* node :
           bookmark_model->GetNodesByURL(url_)) {
        bookmark_model->Remove(node,
                               bookmarks::metrics::BookmarkEditSource::kOther,
                               FROM_HERE);
      }
      break;
    }
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

std::string BookmarkManagementTool::DebugString() const {
  if (action_ == Action::kAdd) {
    return absl::StrFormat(
        "BookmarkManagementTool[action=AddBookmark, url=%s, title=\"%s\"]",
        url_.spec(), base::UTF16ToUTF8(title_));
  }
  return absl::StrFormat(
      "BookmarkManagementTool[action=RemoveBookmark, url=%s]", url_.spec());
}

std::string BookmarkManagementTool::JournalEvent() const {
  switch (action_) {
    case Action::kAdd:
      return "AddBookmark";
    case Action::kRemove:
      return "RemoveBookmark";
  }
}

std::unique_ptr<ObservationDelayController>
BookmarkManagementTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

tabs::TabHandle BookmarkManagementTool::GetTargetTab() const {
  return tabs::TabHandle::Null();
}

}  // namespace actor
