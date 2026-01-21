// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_HELPERS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_HELPERS_H_

namespace performance_manager {

class ProcessNode;

namespace policies {

// If process_node represents a valid process, it sets ProcessPriorityBoost to
// disable_boost.
void SetDisableBoostIfValid(const ProcessNode* process_node,
                            bool disable_boost);

// Calls ::GetProcessPriorityBoost(). The underlying process must be valid.
bool IsProcessPriorityBoostEnabled(const ProcessNode* process_node);

}  // namespace policies

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_HELPERS_H_
