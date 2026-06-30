// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_CONTENTS_USER_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_CONTENTS_USER_DATA_H_

#include <optional>

#include "base/uuid.h"
#include "components/contextual_search/input_state_model.h"
#include "content/public/browser/web_contents_user_data.h"

namespace contextual_tasks {

class ContextualTasksWebContentsUserData
    : public content::WebContentsUserData<ContextualTasksWebContentsUserData> {
 public:
  ~ContextualTasksWebContentsUserData() override;

  base::WeakPtr<contextual_search::InputStateModel> input_state_model() {
    return input_state_model_ ? input_state_model_->AsWeakPtr() : nullptr;
  }
  void set_input_state_model(
      std::unique_ptr<contextual_search::InputStateModel> input_state_model) {
    input_state_model_ = std::move(input_state_model);
  }

  base::WeakPtr<contextual_search::InputStateModel> GetOrCreateInputStateModel(
      contextual_search::ContextualSearchSessionHandle& session_handle);

  const std::optional<base::Uuid>& pending_task_id() const {
    return pending_task_id_;
  }
  void set_pending_task_id(const std::optional<base::Uuid>& pending_task_id) {
    pending_task_id_ = pending_task_id;
  }

 private:
  explicit ContextualTasksWebContentsUserData(content::WebContents* contents);
  friend class content::WebContentsUserData<ContextualTasksWebContentsUserData>;

  std::unique_ptr<contextual_search::InputStateModel> input_state_model_;
  // A pending task associated with this web contents.
  std::optional<base::Uuid> pending_task_id_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_WEB_CONTENTS_USER_DATA_H_
