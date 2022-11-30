// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/prefs_ash_observer.h"

#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefsAshObserver, LocalStateUpdatedOnChange) {
  base::test::TaskEnvironment task_environment;

  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  local_state.Get()->SetString(prefs::kDnsOverHttpsMode, "automatic");
  local_state.Get()->SetString(prefs::kDnsOverHttpsTemplates, "");

  PrefsAshObserver observer(local_state.Get());
  observer.OnDnsOverHttpsModeChanged(base::Value("off"));
  EXPECT_EQ("off", local_state.Get()->GetString(prefs::kDnsOverHttpsMode));

  observer.OnDnsOverHttpsTemplatesChanged(
      base::Value("https://dns.google/dns-query{?dns}"));
  EXPECT_EQ("https://dns.google/dns-query{?dns}",
            local_state.Get()->GetString(prefs::kDnsOverHttpsTemplates));
}
