// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/performance_manager/persistence/site_data/leveldb_site_data_store.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/unittest_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

using SiteDataCacheFacadeTest = testing::TestWithPerformanceManager;

TEST_F(SiteDataCacheFacadeTest, IsDataCacheRecordingForTesting) {
  // Create the SiteDataCacheFactory instance and pass it to the PM sequence for
  // ownership.
  PerformanceManagerImpl::GetInstance()->CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<SiteDataCacheFactory> site_data_cache_factory,
             performance_manager::GraphImpl* graph) {
            graph->PassToGraph(std::move(site_data_cache_factory));
          },
          std::make_unique<SiteDataCacheFactory>()));

  TestingProfile profile;
  // Uses an in-memory database.
  auto use_in_memory_db_for_testing =
      LevelDBSiteDataStore::UseInMemoryDBForTesting();

  bool cache_is_recording = false;

  SiteDataCacheFacade data_cache_facade(&profile);
  data_cache_facade.WaitUntilCacheInitializedForTesting();
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    data_cache_facade.IsDataCacheRecordingForTesting(
        base::BindLambdaForTesting([&](bool is_recording) {
          cache_is_recording = is_recording;
          std::move(quit_closure).Run();
        }));
    run_loop.Run();
  }
  EXPECT_TRUE(cache_is_recording);

  SiteDataCacheFacade off_record_data_cache_facade(
      profile.GetOffTheRecordProfile());
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    off_record_data_cache_facade.IsDataCacheRecordingForTesting(
        base::BindLambdaForTesting([&](bool is_recording) {
          cache_is_recording = is_recording;
          quit_closure.Run();
        }));
    run_loop.Run();
  }

  EXPECT_FALSE(cache_is_recording);
}

}  // namespace performance_manager
