// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEB_APPS_SYNC_TEST_BASE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEB_APPS_SYNC_TEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

namespace web_app {

class WebAppsSyncTestBase : public SyncTest {
 public:
  explicit WebAppsSyncTestBase(TestType test_type);
  ~WebAppsSyncTestBase() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEB_APPS_SYNC_TEST_BASE_H_
