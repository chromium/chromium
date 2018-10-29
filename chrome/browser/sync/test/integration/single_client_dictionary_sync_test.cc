// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace {

class SingleClientDictionarySyncTest : public FeatureToggler, public SyncTest {
 public:
  SingleClientDictionarySyncTest()
      : FeatureToggler(switches::kSyncPseudoUSSDictionary),
        SyncTest(SINGLE_CLIENT) {}
  ~SingleClientDictionarySyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientDictionarySyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientDictionarySyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  std::string word = "foo";
  ASSERT_TRUE(dictionary_helper::AddWord(0, word));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());

  ASSERT_TRUE(dictionary_helper::RemoveWord(0, word));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(dictionary_helper::DictionariesMatch());
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientDictionarySyncTest,
                        ::testing::Values(false, true));

}  // namespace
