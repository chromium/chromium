// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/service/glic_activity_types.h"

#include <string>
#include <utility>

namespace glic {

GlicActivityKey::GlicActivityKey(GlicActivityType type, std::string id)
    : type(type), id(std::move(id)) {}

GlicActivityItem::GlicActivityItem(GlicActivityKey key, GlicActivityState state)
    : key(std::move(key)), state(state), timestamp(base::Time::Now()) {}

GlicActivityItem::GlicActivityItem(const GlicActivityItem&) = default;
GlicActivityItem& GlicActivityItem::operator=(const GlicActivityItem&) =
    default;
GlicActivityItem::GlicActivityItem(GlicActivityItem&&) = default;
GlicActivityItem& GlicActivityItem::operator=(GlicActivityItem&&) = default;
GlicActivityItem::~GlicActivityItem() = default;

}  // namespace glic
