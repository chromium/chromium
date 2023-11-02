// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"

#include <algorithm>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "url/gurl.h"

namespace policy {

namespace {
static base::NoDestructor<base::flat_map<GURL, DlpContentRestrictionSet>>
    g_restrictions_for_url_for_testing;
}

DlpContentRestrictionSet::DlpContentRestrictionSet() {
  restrictions_.fill(RestrictionLevelAndUrl());
}

DlpContentRestrictionSet::DlpContentRestrictionSet(
    DlpContentRestriction restriction,
    DlpRulesManager::Level level) {
  restrictions_.fill(RestrictionLevelAndUrl());
  restrictions_[static_cast<int>(restriction)].level = level;
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
                                              DlpRulesManager::Level level,
                                              const GURL& url) {
  if (level > restrictions_[static_cast<int>(restriction)].level) {
    restrictions_[static_cast<int>(restriction)] =
        RestrictionLevelAndUrl(level, url);
  }
}

DlpRulesManager::Level DlpContentRestrictionSet::GetRestrictionLevel(
    DlpContentRestriction restriction) const {
  return restrictions_[static_cast<int>(restriction)].level;
}

const GURL& DlpContentRestrictionSet::GetRestrictionUrl(
    DlpContentRestriction restriction) const {
  return restrictions_[static_cast<int>(restriction)].url;
}

RestrictionLevelAndUrl DlpContentRestrictionSet::GetRestrictionLevelAndUrl(
    DlpContentRestriction restriction) const {
  return restrictions_[static_cast<int>(restriction)];
}

bool DlpContentRestrictionSet::IsEmpty() const {
  for (size_t i = 0; i < restrictions_.size(); ++i) {
    if (restrictions_[i].level != DlpRulesManager::Level::kNotSet)
      return false;
  }
  return true;
}

void DlpContentRestrictionSet::UnionWith(
    const DlpContentRestrictionSet& other) {
  for (size_t i = 0; i < restrictions_.size(); ++i) {
    if (other.restrictions_[i].level > restrictions_[i].level) {
      restrictions_[i] = other.restrictions_[i];
    }
  }
}

DlpContentRestrictionSet DlpContentRestrictionSet::DifferenceWith(
    const DlpContentRestrictionSet& other) const {
  // Leave only the restrictions that are present in |this|, but not in |other|.
  DlpContentRestrictionSet result;
  for (size_t i = 0; i < restrictions_.size(); ++i) {
    if (restrictions_[i].level > other.restrictions_[i].level) {
      result.restrictions_[i] = restrictions_[i];
    }
  }
  return result;
}

// static
DlpContentRestrictionSet DlpContentRestrictionSet::GetForURL(const GURL& url) {
  if (g_restrictions_for_url_for_testing->find(url) !=
      g_restrictions_for_url_for_testing->end()) {
    return g_restrictions_for_url_for_testing->at(url);
  }

  DlpContentRestrictionSet set;

  DlpRulesManager* dlp_rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!dlp_rules_manager)
    return set;

  const size_t kRestrictionsCount = 4;
  static constexpr std::array<
      std::pair<DlpRulesManager::Restriction, DlpContentRestriction>,
      kRestrictionsCount>
      kRestrictionsArray = {{{DlpRulesManager::Restriction::kScreenshot,
                              DlpContentRestriction::kScreenshot},
                             {DlpRulesManager::Restriction::kPrivacyScreen,
                              DlpContentRestriction::kPrivacyScreen},
                             {DlpRulesManager::Restriction::kPrinting,
                              DlpContentRestriction::kPrint},
                             {DlpRulesManager::Restriction::kScreenShare,
                              DlpContentRestriction::kScreenShare}}};

  for (const auto& restriction : kRestrictionsArray) {
    DlpRulesManager::Level level =
        dlp_rules_manager->IsRestricted(url, restriction.first);
    if (level == DlpRulesManager::Level::kNotSet ||
        level == DlpRulesManager::Level::kAllow)
      continue;
    set.SetRestriction(restriction.second, level, url);
  }

  return set;
}

// static
void DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
    const GURL& url,
    const DlpContentRestrictionSet& restrictions) {
  g_restrictions_for_url_for_testing->insert_or_assign(url, restrictions);
}

}  // namespace policy
