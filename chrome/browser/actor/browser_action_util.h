// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_
#define CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_

#include <memory>

// Conversion function for turning optimization_guide::proto::* types into
// ToolRequests usable by the actor framework.
// TODO(bokan): Rename to actor_proto_conversion.h|cc

namespace optimization_guide::proto {
class Action;
}  // namespace optimization_guide::proto

namespace tabs {
class TabInterface;
}

namespace actor {
class ToolRequest;

// Build a ToolRequest from the provided optimization_guide Action proto. If the
// action proto doesn't provide a tab_id, and the fallback_tab parameter is
// provided (non-null), the fallback_tab will be used as the acting tab.
// However, this parameter will eventually be phased out and clients will be
// expected to always provide a tab id on each Action. Returns nullptr if the
// action is invalid.
// TODO(https://crbug.com/411462297): The client should eventually always
// provide a tab id for actions where one is needed. Remove this parameter when
// that's done.
std::unique_ptr<ToolRequest> CreateToolRequest(
    const optimization_guide::proto::Action& action,
    tabs::TabInterface* deprecated_fallback_tab);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_
