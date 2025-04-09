// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_

#include <string_view>

#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

inline constexpr char kActorTestDataPath[] = "chrome/test/data/actor";

optimization_guide::proto::BrowserAction MakeClick(int content_node_id);
optimization_guide::proto::BrowserAction MakeHistoryBack();
optimization_guide::proto::BrowserAction MakeHistoryForward();
optimization_guide::proto::BrowserAction MakeMouseMove(int content_node_id);
optimization_guide::proto::BrowserAction MakeNavigate(
    std::string_view target_url);
optimization_guide::proto::BrowserAction MakeType(int content_node_id,
                                                  std::string_view text,
                                                  bool follow_by_enter);
optimization_guide::proto::BrowserAction MakeScroll(
    std::optional<int> content_node_id,
    float scroll_offset_x,
    float scroll_offset_y);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TEST_UTIL_H_
