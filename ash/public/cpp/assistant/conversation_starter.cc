// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/conversation_starter.h"

namespace ash {

ConversationStarter::ConversationStarter(const std::string& label,
                                         const absl::optional<GURL>& action_url,
                                         const absl::optional<GURL>& icon_url,
                                         uint32_t required_permissions)
    : label_(label),
      action_url_(action_url),
      icon_url_(icon_url),
      required_permissions_(required_permissions) {}

ConversationStarter::ConversationStarter(const ConversationStarter& copy) =
    default;

ConversationStarter::~ConversationStarter() = default;

bool ConversationStarter::RequiresPermission(Permission permission) const {
  return (required_permissions_ & permission) == permission;
}

}  // namespace ash
