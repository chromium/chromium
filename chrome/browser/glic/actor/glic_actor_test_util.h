// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TEST_UTIL_H_
#define CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TEST_UTIL_H_

namespace actor {
struct TaskSourceInfo;
}  // namespace actor

namespace glic {

// Returns a common mock TaskSourceInfo used by actor tests.
const ::actor::TaskSourceInfo& MockGlicTaskSourceInfo();

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TEST_UTIL_H_
