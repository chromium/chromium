// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/initial_external_extension_loader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

using ::testing::Optional;

constexpr char kValidIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kValidIdB[] = "abacabadabacabadabacabadabacabac";
constexpr char kInvalidId[] = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";

std::string MakePrefName(const std::string_view id,
                         const std::string_view leaf) {
  return base::StrCat({id, ".", leaf});
}

// Subclass to capture LoadFinished payloads.
class TestInitialExternalExtensionLoader
    : public InitialExternalExtensionLoader {
 public:
  explicit TestInitialExternalExtensionLoader(PrefService& prefs)
      : InitialExternalExtensionLoader(prefs) {}

  TestInitialExternalExtensionLoader(
      const TestInitialExternalExtensionLoader&) = delete;
  TestInitialExternalExtensionLoader& operator=(
      const TestInitialExternalExtensionLoader&) = delete;

  base::Value::Dict WaitForLoadFinished() {
    if (!saw_load_) {
      load_loop_.Run();
    }
    saw_load_ = false;
    return std::move(last_loaded_prefs_);
  }

  int load_finished_count() const { return load_finished_count_; }

 protected:
  ~TestInitialExternalExtensionLoader() override = default;

  void LoadFinished(base::Value::Dict prefs) override {
    ++load_finished_count_;
    last_loaded_prefs_ = prefs.Clone();
    saw_load_ = true;
    load_loop_.Quit();
    InitialExternalExtensionLoader::LoadFinished(std::move(prefs));
  }

 private:
  int load_finished_count_ = 0;
  base::RunLoop load_loop_;
  bool saw_load_ = false;
  base::Value::Dict last_loaded_prefs_;
};

class InitialExternalExtensionLoaderTest : public ::testing::Test {
 protected:
  InitialExternalExtensionLoaderTest() = default;
  ~InitialExternalExtensionLoaderTest() override = default;

  void SetUp() override {
    prefs_.registry()->RegisterListPref(pref_names::kInitialInstallList);
  }

  void SetInitialIds(const std::vector<std::string>& ids) {
    base::Value::List list;
    for (const auto& id : ids) {
      list.Append(id);
    }
    prefs_.SetList(pref_names::kInitialInstallList, std::move(list));
  }

  TestingPrefServiceSimple prefs_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(InitialExternalExtensionLoaderTest, StartLoadingProducesPrefs) {
  SetInitialIds({kValidIdA, kValidIdB});

  scoped_refptr<TestInitialExternalExtensionLoader> loader =
      base::MakeRefCounted<TestInitialExternalExtensionLoader>(prefs_);

  loader->StartLoading();
  base::Value::Dict prefs = loader->WaitForLoadFinished();

  const std::string expected_update_url =
      extension_urls::GetWebstoreUpdateUrl().spec();

  for (const char* id : {kValidIdA, kValidIdB}) {
    EXPECT_THAT(*prefs.FindStringByDottedPath(
                    MakePrefName(id, ExternalProviderImpl::kExternalUpdateUrl)),
                testing::Eq(expected_update_url));
    const std::optional<bool> untrusted = prefs.FindBoolByDottedPath(
        MakePrefName(id, ExternalProviderImpl::kMayBeUntrusted));
    ASSERT_TRUE(untrusted.has_value());
  }

  EXPECT_EQ(loader->load_finished_count(), 1);
}

TEST_F(InitialExternalExtensionLoaderTest, IgnoresInvalidIds) {
  SetInitialIds({kValidIdA, kInvalidId});

  scoped_refptr<TestInitialExternalExtensionLoader> loader =
      base::MakeRefCounted<TestInitialExternalExtensionLoader>(prefs_);

  loader->StartLoading();
  base::Value::Dict prefs = loader->WaitForLoadFinished();

  EXPECT_NE(nullptr, prefs.FindStringByDottedPath(MakePrefName(
                         kValidIdA, ExternalProviderImpl::kExternalUpdateUrl)));
  EXPECT_TRUE(prefs
                  .FindBoolByDottedPath(MakePrefName(
                      kValidIdA, ExternalProviderImpl::kMayBeUntrusted))
                  .has_value());

  EXPECT_EQ(nullptr,
            prefs.FindStringByDottedPath(MakePrefName(
                kInvalidId, ExternalProviderImpl::kExternalUpdateUrl)));
  EXPECT_FALSE(prefs
                   .FindBoolByDottedPath(MakePrefName(
                       kInvalidId, ExternalProviderImpl::kMayBeUntrusted))
                   .has_value());
}

TEST_F(InitialExternalExtensionLoaderTest, EmptyListProducesEmptyPrefs) {
  SetInitialIds({});

  scoped_refptr<TestInitialExternalExtensionLoader> loader =
      base::MakeRefCounted<TestInitialExternalExtensionLoader>(prefs_);

  loader->StartLoading();
  base::Value::Dict prefs = loader->WaitForLoadFinished();

  EXPECT_TRUE(prefs.empty());
  EXPECT_EQ(loader->load_finished_count(), 1);
}

TEST_F(InitialExternalExtensionLoaderTest, ReloadAfterPrefChange) {
  SetInitialIds({kValidIdA, kValidIdB});
  scoped_refptr<TestInitialExternalExtensionLoader> loader =
      base::MakeRefCounted<TestInitialExternalExtensionLoader>(prefs_);

  loader->StartLoading();
  (void)loader->WaitForLoadFinished();

  // Shrink the list, then trigger another load.
  SetInitialIds({kValidIdB});
  loader->StartLoading();
  base::Value::Dict prefs2 = loader->WaitForLoadFinished();

  EXPECT_NE(nullptr, prefs2.FindStringByDottedPath(MakePrefName(
                         kValidIdB, ExternalProviderImpl::kExternalUpdateUrl)));
  EXPECT_EQ(nullptr, prefs2.FindStringByDottedPath(MakePrefName(
                         kValidIdA, ExternalProviderImpl::kExternalUpdateUrl)));
}

}  // namespace
}  // namespace extensions
