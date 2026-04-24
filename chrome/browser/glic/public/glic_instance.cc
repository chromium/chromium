// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_instance.h"

#include "base/strings/string_number_conversions.h"

namespace glic {

GlicInstance::~GlicInstance() = default;

InstanceId InstanceId::Create(uint64_t glic_instance_coordinator_id,
                              uint32_t index) {
  return InstanceId(
      base::StrCat({base::NumberToString(glic_instance_coordinator_id), "-",
                    base::NumberToString(index)}));
}

ConversationInfo::ConversationInfo() = default;
ConversationInfo::ConversationInfo(InstanceId instance_id, std::string title)
    : instance_id(std::move(instance_id)), title(std::move(title)) {}
ConversationInfo::~ConversationInfo() = default;
ConversationInfo::ConversationInfo(const ConversationInfo&) = default;
ConversationInfo& ConversationInfo::operator=(const ConversationInfo&) =
    default;

}  // namespace glic
