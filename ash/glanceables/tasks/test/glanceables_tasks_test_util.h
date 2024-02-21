// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_TEST_GLANCEABLES_TASKS_TEST_UTIL_H_
#define ASH_GLANCEABLES_TASKS_TEST_GLANCEABLES_TASKS_TEST_UTIL_H_

#include <memory>

#include "ash/api/tasks/fake_tasks_client.h"

namespace ash::glanceables_tasks_test_util {

// Creates and pre-loads initial values into a `FakeTasksClient` object for use
// in Glanceables tests.
std::unique_ptr<api::FakeTasksClient> InitializeFakeTasksClient(
    const base::Time& tasks_time);

}  // namespace ash::glanceables_tasks_test_util

#endif  // ASH_GLANCEABLES_TASKS_TEST_GLANCEABLES_TASKS_TEST_UTIL_H_
