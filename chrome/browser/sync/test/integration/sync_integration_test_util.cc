// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"

#include "chrome/browser/sync/test/integration/themes_helper.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/test_utils.h"

void SetCustomTheme(Profile* profile, int theme_index) {
  test::ThemeServiceChangedWaiter waiter(
      ThemeServiceFactory::GetForProfile(profile));
  themes_helper::UseCustomTheme(profile, theme_index);
  waiter.WaitForThemeChanged();
}

ServerCountMatchStatusChecker::ServerCountMatchStatusChecker(
    syncer::DataType type,
    size_t count)
    : type_(type), count_(count) {}

bool ServerCountMatchStatusChecker::IsExitConditionSatisfied(std::ostream* os) {
  size_t actual_count = fake_server()->GetSyncEntitiesByDataType(type_).size();
  *os << "Waiting for fake server entity count " << actual_count
      << " to match expected count " << count_ << " for type "
      << DataTypeToDebugString(type_);
  return count_ == actual_count;
}
