// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_test_util.h"

#include <string_view>

#include "base/no_destructor.h"
#include "components/actor/task_source_info.h"

namespace glic {

const actor::TaskSourceInfo& MockGlicTaskSourceInfo() {
  constexpr std::string_view kMockConversationId = "123456abcdef";
  static base::NoDestructor<actor::TaskSourceInfo> task_source_info(
      actor::TaskSourceInfo::Client::kGlic, std::string(kMockConversationId));
  return *task_source_info.get();
}

}  // namespace glic
