// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_TEST_UTILS_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_TEST_UTILS_H_

#include <string>

class AccountId;

namespace ash {

namespace api {
class FakeTasksClient;
}  // namespace api

// Creates and installs a new FakeTasksClient. The client is owned by
// `TestTasksDelegate`.
api::FakeTasksClient& CreateFakeTasksClient(const AccountId& account_id);

// Utility functions to add sample tasks to the fake tasks client. If more
// control is needed, you can create `api::TaskList` and `api::Task` objects
// directly and add them to the client.
void AddFakeTaskList(api::FakeTasksClient& client,
                     const std::string& task_list_id);
// Adds a fake task. Note that the referenced task list must exist first.
void AddFakeTask(api::FakeTasksClient& client,
                 const std::string& task_list_id,
                 const std::string& task_id,
                 const std::string& title);

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_TASK_TEST_UTILS_H_
