// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_policy_loader.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using extensions::mojom::ManifestLocation;

namespace extensions {

class ExternalPolicyLoaderTest : public testing::Test {
 public:
  ExternalPolicyLoaderTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  ~ExternalPolicyLoaderTest() override = default;

 private:
  // Needed to satisfy BrowserThread::CurrentlyOn(BrowserThread::UI) checks in
  // ExternalProviderImpl.
  content::BrowserTaskEnvironment task_environment_;
};

class MockExternalPolicyProviderVisitor
    : public ExternalProviderInterface::VisitorInterface {
 public:
  MockExternalPolicyProviderVisitor() = default;

  MockExternalPolicyProviderVisitor(const MockExternalPolicyProviderVisitor&) =
      delete;
  MockExternalPolicyProviderVisitor& operator=(
      const MockExternalPolicyProviderVisitor&) = delete;

  // Initialize a provider with |policy_forcelist|, and check that it installs
  // exactly the extensions specified in |expected_extensions|.
  void Visit(const base::Value::Dict& policy_forcelist,
             const std::set<std::string>& expected_extensions) {
    profile_ = std::make_unique<TestingProfile>();
    profile_->GetTestingPrefService()->SetManagedPref(
        pref_names::kInstallForceList, base::Value(policy_forcelist.Clone()));
    provider_ = std::make_unique<ExternalProviderImpl>(
        this,
        new ExternalPolicyLoader(
            profile_.get(),
            ExtensionManagementFactory::GetForBrowserContext(profile_.get()),
            ExternalPolicyLoader::FORCED),
        profile_.get(), ManifestLocation::kInvalidLocation,
        ManifestLocation::kExternalPolicyDownload, Extension::NO_FLAGS);

    // Extensions will be removed from this list as they visited,
    // so it should be emptied by the end.
    expected_extensions_ = expected_extensions;
    provider_->VisitRegisteredExtension();
    EXPECT_TRUE(expected_extensions_.empty());
  }

  bool OnExternalExtensionFileFound(
      const ExternalInstallInfoFile& info) override {
    ADD_FAILURE() << "There should be no external extensions from files.";
    return false;
  }

  bool OnExternalExtensionUpdateUrlFound(
      const ExternalInstallInfoUpdateUrl& info,
      bool force_update) override {
    // Extension has the correct location.
    EXPECT_EQ(ManifestLocation::kExternalPolicyDownload,
              info.download_location);

    // Provider returns the correct location when asked.
    ManifestLocation location1;
    std::unique_ptr<base::Version> version1;
    provider_->GetExtensionDetails(info.extension_id, &location1, &version1);
    EXPECT_EQ(ManifestLocation::kExternalPolicyDownload, location1);
    EXPECT_FALSE(version1.get());

    // Remove the extension from our list.
    EXPECT_EQ(1U, expected_extensions_.erase(info.extension_id));
    return true;
  }

  void OnExternalProviderReady(
      const ExternalProviderInterface* provider) override {
    EXPECT_EQ(provider, provider_.get());
    EXPECT_TRUE(provider->IsReady());
  }

  void OnExternalProviderUpdateComplete(
      const ExternalProviderInterface* provider,
      const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const std::vector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    ADD_FAILURE() << "Only win registry provider is expected to call this.";
  }

 private:
  std::set<std::string> expected_extensions_;

  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<ExternalProviderImpl> provider_;
};

TEST_F(ExternalPolicyLoaderTest, PolicyIsParsed) {
  base::Value::Dict forced_extensions;
  std::set<std::string> expected_extensions;
  ExternalPolicyLoader::AddExtension(forced_extensions,
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                     "http://www.example.com/crx?a=5;b=6");
  expected_extensions.insert("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  ExternalPolicyLoader::AddExtension(
      forced_extensions, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "https://clients2.google.com/service/update2/crx");
  expected_extensions.insert("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

  MockExternalPolicyProviderVisitor mv;
  mv.Visit(forced_extensions, expected_extensions);
}

TEST_F(ExternalPolicyLoaderTest, InvalidEntriesIgnored) {
  base::Value::Dict forced_extensions;
  std::set<std::string> expected_extensions;

  ExternalPolicyLoader::AddExtension(forced_extensions,
                                     "cccccccccccccccccccccccccccccccc",
                                     "http://www.example.com/crx");
  expected_extensions.insert("cccccccccccccccccccccccccccccccc");

  // Add invalid entries.
  forced_extensions.Set("invalid", "http://www.example.com/crx");
  forced_extensions.Set("dddddddddddddddddddddddddddddddd", std::string());
  forced_extensions.Set("invalid", "bad");

  MockExternalPolicyProviderVisitor mv;
  mv.Visit(forced_extensions, expected_extensions);
}
}  // namespace extensions
