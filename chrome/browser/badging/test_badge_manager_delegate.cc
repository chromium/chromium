// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/test_badge_manager_delegate.h"

#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"

namespace badging {

TestBadgeManagerDelegate::TestBadgeManagerDelegate(Profile* profile,
                                                   BadgeManager* badge_manager)
    : BadgeManagerDelegate(profile, badge_manager) {}

TestBadgeManagerDelegate::~TestBadgeManagerDelegate() = default;

void TestBadgeManagerDelegate::SetOnBadgeChanged(
    base::RepeatingCallback<void()> on_badge_changed) {
  on_badge_changed_ = on_badge_changed;
}

void TestBadgeManagerDelegate::OnAppBadgeUpdated(const web_app::AppId& app_id) {
  const auto& value = badge_manager()->GetBadgeValue(app_id);
  if (!value)
    cleared_badges_.push_back(app_id);
  else
    set_badges_.push_back(std::make_pair(app_id, value.value()));

  if (on_badge_changed_)
    on_badge_changed_.Run();
}

void TestBadgeManagerDelegate::ResetBadges() {
  cleared_badges_.clear();
  set_badges_.clear();
}

}  // namespace badging
