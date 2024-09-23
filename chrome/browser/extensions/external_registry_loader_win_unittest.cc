// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_registry_loader_win.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kDummyRegistryKey[] = "dummyId";

class TestExternalRegistryLoader : public ExternalRegistryLoader {
 public:
  TestExternalRegistryLoader() {}

  TestExternalRegistryLoader(const TestExternalRegistryLoader&) = delete;
  TestExternalRegistryLoader& operator=(const TestExternalRegistryLoader&) =
      delete;

  using ExternalRegistryLoader::StartLoading;

  void WaitForTwoLoadsToFinished() {
    // Run() returns immediately if Quit() has already been called.
    run_loop_.Run();
  }

  std::vector<int> GetPrefsTestIds() { return prefs_test_ids_; }

 private:
  ~TestExternalRegistryLoader() override {}

  base::Value::Dict LoadPrefsOnBlockingThread() override {
    return base::Value::Dict().Set(kDummyRegistryKey, id_++);
  }
  void LoadFinished(base::Value::Dict prefs) override {
    ++load_finished_count_;
    ASSERT_LE(load_finished_count_, 2);

    auto prefs_test_id = prefs.FindInt(kDummyRegistryKey);
    ASSERT_TRUE(prefs_test_id.has_value());
    prefs_test_ids_.push_back(*prefs_test_id);

    ExternalRegistryLoader::LoadFinished(std::move(prefs));

    if (load_finished_count_ == 2) {
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
  int load_finished_count_ = 0;
  int id_ = 0;
  std::vector<int> prefs_test_ids_;
};

}  // namespace

class ExternalRegistryLoaderUnittest : public testing::Test {
 public:
  ExternalRegistryLoaderUnittest() = default;

  ExternalRegistryLoaderUnittest(const ExternalRegistryLoaderUnittest&) =
      delete;
  ExternalRegistryLoaderUnittest& operator=(
      const ExternalRegistryLoaderUnittest&) = delete;

  ~ExternalRegistryLoaderUnittest() override {}

 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
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
