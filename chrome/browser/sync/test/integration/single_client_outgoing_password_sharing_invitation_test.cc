// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/test/browser_test.h"

namespace {

class SingleClientOutgoingPasswordSharingInvitationTest : public SyncTest {
 public:
  SingleClientOutgoingPasswordSharingInvitationTest()
      : SyncTest(SINGLE_CLIENT) {}

 private:
  base::test::ScopedFeatureList override_features_{
      password_manager::features::kPasswordManagerEnableSenderService};
};

IN_PROC_BROWSER_TEST_F(SingleClientOutgoingPasswordSharingInvitationTest,
                       SanityCheck) {
  ASSERT_TRUE(SetupSync());
}

}  // namespace
