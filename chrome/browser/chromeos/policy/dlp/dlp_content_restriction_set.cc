// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"

#include <algorithm>

namespace policy {

DlpContentRestrictionSet::DlpContentRestrictionSet() {
  restrictions_.fill(DlpRulesManager::Level::kNotSet);
}

DlpContentRestrictionSet::DlpContentRestrictionSet(
    DlpContentRestriction restriction,
    DlpRulesManager::Level level) {
  restrictions_.fill(DlpRulesManager::Level::kNotSet);
  restrictions_[restriction] = level;
}

DlpContentRestrictionSet::DlpContentRestrictionSet(
    const DlpContentRestrictionSet& restriction_set) = default;

DlpContentRestrictionSet& DlpContentRestrictionSet::operator=(
    const DlpContentRestrictionSet& other) = default;

DlpContentRestrictionSet::~DlpContentRestrictionSet() = default;

bool DlpContentRestrictionSet::operator==(
    const DlpContentRestrictionSet& other) const {
  return restrictions_ == other.restrictions_;
}

bool DlpContentRestrictionSet::operator!=(
    const DlpContentRestrictionSet& other) const {
  return !(*this == other);
}

void DlpContentRestrictionSet::SetRestriction(DlpContentRestriction restriction,
                                              DlpRulesManager::Level level) {
  restrictions_[restriction] = std::max(restrictions_[restriction], level);
}

DlpRulesManager::Level DlpContentRestrictionSet::GetRestriction(
    DlpContentRestriction restriction) const {
  return restrictions_[restriction];
}

bool DlpContentRestrictionSet::IsEmpty() const {
  for (int i = 0; i < restrictions_.size(); ++i) {
    if (restrictions_[i] != DlpRulesManager::Level::kNotSet)
      return false;
  }
  return true;
}

void DlpContentRestrictionSet::UnionWith(
    const DlpContentRestrictionSet& other) {
  for (int i = 0; i < restrictions_.size(); ++i) {
    restrictions_[i] = std::max(restrictions_[i], other.restrictions_[i]);
  }
}

DlpContentRestrictionSet DlpContentRestrictionSet::DifferenceWith(
    const DlpContentRestrictionSet& other) const {
  // Leave only the restrictions that are present in |this|, but not in |other|.
  DlpContentRestrictionSet result;
  for (int i = 0; i < restrictions_.size(); ++i) {
    if (restrictions_[i] > other.restrictions_[i]) {
      result.restrictions_[i] = restrictions_[i];
    }
  }
  return result;
}

}  // namespace policy
