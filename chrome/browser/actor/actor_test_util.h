// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_

#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

optimization_guide::proto::BrowserAction MakeClick(int content_node_id);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_
