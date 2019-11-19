// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_registry_loader_win.h"

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kDummyRegistryKey[] = "dummyId";

class TestExternalRegistryLoader : public ExternalRegistryLoader {
 public:
  TestExternalRegistryLoader() {}

  using ExternalRegistryLoader::StartLoading;

  void WaitForTwoLoadsToFinished() {
    // Run() returns immediately if Quit() has already been called.
    run_loop_.Run();
  }

  std::vector<int> GetPrefsTestIds() { return prefs_test_ids_; }

 private:
  ~TestExternalRegistryLoader() override {}

  std::unique_ptr<base::DictionaryValue> LoadPrefsOnBlockingThread() override {
    return DictionaryBuilder().Set(kDummyRegistryKey, id_++).Build();
  }
  void LoadFinished(std::unique_ptr<base::DictionaryValue> prefs) override {
    ++load_finished_count_;
    ASSERT_LE(load_finished_count_, 2);

    ASSERT_TRUE(prefs);
    int prefs_test_id = -1;
    EXPECT_TRUE(prefs->GetInteger(kDummyRegistryKey, &prefs_test_id));
    prefs_test_ids_.push_back(prefs_test_id);

    ExternalRegistryLoader::LoadFinished(std::move(prefs));

    if (load_finished_count_ == 2)
      run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  int load_finished_count_ = 0;
  int id_ = 0;
  std::vector<int> prefs_test_ids_;

  DISALLOW_COPY_AND_ASSIGN(TestExternalRegistryLoader);
};

}  // namespace

class ExternalRegistryLoaderUnittest : public testing::Test {
 public:
  ExternalRegistryLoaderUnittest() {}
  ~ExternalRegistryLoaderUnittest() override {}

 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ExternalRegistryLoaderUnittest);
};

// Tests that calling StartLoading() more than once doesn't fail DCHECK.
// Regression test for https://crbug.com/653045.
TEST_F(ExternalRegistryLoaderUnittest, TwoStartLoadingDoesNotCrash) {
  scoped_refptr<TestExternalRegistryLoader> test_loader =
      base::MakeRefCounted<TestExternalRegistryLoader>();

  test_loader->StartLoading();
  test_loader->StartLoading();

  test_loader->WaitForTwoLoadsToFinished();
  // Let registry watcher code complete.
  RunUntilIdle();
}

// Tests that calling StartLoading() twice does not overwrite previous prefs
// before LoadFinished consumes it.
// Regression test for https://crbug.com/709304: if two StartLoading() schedules
// two LoadPrefsOnBlockingThread, then the second LoadPrefsOnBlockingThread
// could overwrite the first one's prefs.
TEST_F(ExternalRegistryLoaderUnittest, TwoStartLoadingDoesNotOverwritePrefs) {
  scoped_refptr<TestExternalRegistryLoader> test_loader =
      base::MakeRefCounted<TestExternalRegistryLoader>();

  test_loader->StartLoading();
  test_loader->StartLoading();

  test_loader->WaitForTwoLoadsToFinished();
  // Let registry watcher code complete.
  RunUntilIdle();

  std::vector<int> prefs_test_ids = test_loader->GetPrefsTestIds();
  EXPECT_THAT(prefs_test_ids, testing::ElementsAre(0, 1));
}

}  // namespace extensions
