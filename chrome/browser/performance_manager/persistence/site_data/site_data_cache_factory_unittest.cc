// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_factory.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/performance_manager/persistence/site_data/unittest_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

using SiteDataCacheFactoryTest = testing::TestWithPerformanceManager;

TEST_F(SiteDataCacheFactoryTest, EndToEnd) {
  std::unique_ptr<SiteDataCacheFactory> factory =
      std::make_unique<SiteDataCacheFactory>();
  SiteDataCacheFactory* factory_raw = factory.get();
  PerformanceManagerImpl::GetInstance()->CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<SiteDataCacheFactory> site_data_cache_factory,
             performance_manager::GraphImpl* graph) {
            graph->PassToGraph(std::move(site_data_cache_factory));
          },
          std::move(factory)));

  TestingProfile profile;
  SiteDataCacheFactory::OnBrowserContextCreatedOnUIThread(factory_raw, &profile,
                                                          nullptr);

  {
    base::RunLoop run_loop;
    PerformanceManagerImpl::GetInstance()->CallOnGraphImpl(
        FROM_HERE,
        base::BindOnce(
            [](SiteDataCacheFactory* factory,
               const std::string& browser_context_id,
               base::OnceClosure quit_closure,
               performance_manager::GraphImpl* graph_unused) {
              DCHECK_NE(nullptr, factory->GetDataCacheForBrowserContext(
                                     browser_context_id));
              DCHECK_NE(nullptr, factory->GetInspectorForBrowserContext(
                                     browser_context_id));
              std::move(quit_closure).Run();
            },
            base::Unretained(factory_raw), profile.UniqueId(),
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  SiteDataCacheFactory::OnBrowserContextDestroyedOnUIThread(factory_raw,
                                                            &profile);
  {
    base::RunLoop run_loop;
    PerformanceManagerImpl::GetInstance()->CallOnGraphImpl(
        FROM_HERE,
        base::BindOnce(
            [](SiteDataCacheFactory* factory,
               const std::string& browser_context_id,
               base::OnceClosure quit_closure,
               performance_manager::GraphImpl* graph_unused) {
              DCHECK_EQ(nullptr, factory->GetDataCacheForBrowserContext(
                                     browser_context_id));
              DCHECK_EQ(nullptr, factory->GetInspectorForBrowserContext(
                                     browser_context_id));
              std::move(quit_closure).Run();
            },
            base::Unretained(factory_raw), profile.UniqueId(),
            run_loop.QuitClosure()));
    run_loop.Run();
  }
}

}  // namespace performance_manager
