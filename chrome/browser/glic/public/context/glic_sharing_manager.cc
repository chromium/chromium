// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/context/glic_sharing_manager.h"

namespace glic {

bool GlicSharingManager::PinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  return PinTabs(tab_handles, GlicPinTrigger::kUnknown);
}

bool GlicSharingManager::UnpinTabs(
    base::span<const tabs::TabHandle> tab_handles) {
  return UnpinTabs(tab_handles, GlicUnpinTrigger::kUnknown);
}

void GlicSharingManager::UnpinAllTabs() {
  UnpinAllTabs(GlicUnpinTrigger::kUnknown);
}

GlicPinEvent::GlicPinEvent(GlicPinTrigger trigger, base::TimeTicks timestamp)
    : trigger(trigger), timestamp(timestamp) {}

GlicPinEvent::~GlicPinEvent() = default;

GlicPinnedTabUsage::GlicPinnedTabUsage(GlicPinEvent pin_event)
    : pin_event(pin_event) {}
GlicPinnedTabUsage::GlicPinnedTabUsage(GlicPinTrigger trigger,
                                       base::TimeTicks timestamp)
    : pin_event(trigger, timestamp) {}
GlicPinnedTabUsage::GlicPinnedTabUsage(GlicPinnedTabUsage&& other) = default;
GlicPinnedTabUsage::GlicPinnedTabUsage(const GlicPinnedTabUsage&) = default;
GlicPinnedTabUsage& GlicPinnedTabUsage::operator=(GlicPinnedTabUsage&& other) =
    default;
GlicPinnedTabUsage& GlicPinnedTabUsage::operator=(const GlicPinnedTabUsage&) =
    default;

GlicPinnedTabUsage::~GlicPinnedTabUsage() = default;

GlicUnpinEvent::GlicUnpinEvent(GlicUnpinTrigger trigger,
                               GlicPinnedTabUsage usage,
                               base::TimeTicks timestamp)
    : trigger(trigger), usage(usage), timestamp(timestamp) {}

GlicUnpinEvent::~GlicUnpinEvent() = default;

}  // namespace glic
