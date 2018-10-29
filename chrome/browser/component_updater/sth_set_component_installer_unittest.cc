// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sth_set_component_installer.h"

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "components/certificate_transparency/sth_observer.h"
#include "components/certificate_transparency/sth_reporter.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/cert/signed_tree_head.h"
#include "net/test/ct_test_util.h"
#include "services/data_decoder/public/cpp/testing_json_parser.h"
#include "services/network/network_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

class StoringSTHObserver : public certificate_transparency::STHObserver {
 public:
  void NewSTHObserved(const net::ct::SignedTreeHead& sth) override {
    sths[sth.log_id] = sth;
  }

  std::map<std::string, net::ct::SignedTreeHead> sths;
};

class STHSetComponentInstallerTest : public PlatformTest {
 public:
  STHSetComponentInstallerTest()
      : network_service_(std::make_unique<network::NetworkService>(nullptr)) {
    AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();

    network_service_->sth_reporter()->RegisterObserver(&observer_);
  }

  ~STHSetComponentInstallerTest() override {
    network_service_->sth_reporter()->UnregisterObserver(&observer_);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    policy_ = std::make_unique<STHSetComponentInstallerPolicy>();
    policy_->SetNetworkServiceForTesting(network_service_.get());
  }

  void SimulateCrash() {
    network_service_->sth_reporter()->UnregisterObserver(&observer_);
    observer_.sths.clear();
    network_service_.reset();
    network_service_ = std::make_unique<network::NetworkService>(nullptr);
    network_service_->sth_reporter()->RegisterObserver(&observer_);
    policy_->SetNetworkServiceForTesting(network_service_.get());
  }

  void WriteSTHToFile(const std::string& sth_json,
                      const base::FilePath& filename) {
    ASSERT_EQ(static_cast<int32_t>(sth_json.length()),
              base::WriteFile(filename, sth_json.data(), sth_json.length()));
  }

  base::FilePath GetSTHsDir() {
    return temp_dir_.GetPath()
        .Append(FILE_PATH_LITERAL("_platform_specific"))
        .Append(FILE_PATH_LITERAL("all"))
        .Append(FILE_PATH_LITERAL("sths"));
  }

  void CreateSTHsDir(const base::DictionaryValue& manifest,
                     const base::FilePath& sths_dir) {
    ASSERT_FALSE(policy_->VerifyInstallation(manifest, temp_dir_.GetPath()));
    ASSERT_TRUE(base::CreateDirectory(sths_dir));
  }

  void LoadSTHs(const base::DictionaryValue& manifest,
                const base::FilePath& sths_dir) {
    ASSERT_TRUE(policy_->VerifyInstallation(manifest, temp_dir_.GetPath()));

    const base::Version v("1.0");
    policy_->ComponentReady(v, temp_dir_.GetPath(), manifest.CreateDeepCopy());
    // Drain the RunLoop created by the TestBrowserThreadBundle
    thread_bundle_.RunUntilIdle();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<network::NetworkService> network_service_;
  StoringSTHObserver observer_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<STHSetComponentInstallerPolicy> policy_;
  data_decoder::TestingJsonParser::ScopedFactoryOverride factory_override_;

 private:
  DISALLOW_COPY_AND_ASSIGN(STHSetComponentInstallerTest);
};

// Parses valid STH JSON in a file with valid hex encoding of log id.
TEST_F(STHSetComponentInstallerTest, CanLoadAllSTHs) {
  const base::DictionaryValue manifest;
  const base::FilePath sths_dir(GetSTHsDir());
  CreateSTHsDir(manifest, sths_dir);

  const std::string good_sth_json = net::ct::GetSampleSTHAsJson();
  const base::FilePath first_sth_file =
      sths_dir.Append(FILE_PATH_LITERAL("616263.sth"));
  WriteSTHToFile(good_sth_json, first_sth_file);

  const base::FilePath second_sth_file =
      sths_dir.Append(FILE_PATH_LITERAL("610064.sth"));
  WriteSTHToFile(good_sth_json, second_sth_file);

  LoadSTHs(manifest, sths_dir);

  EXPECT_EQ(2u, observer_.sths.size());

  const std::string first_log_id("abc");
  ASSERT_TRUE(observer_.sths.find(first_log_id) != observer_.sths.end());
  const net::ct::SignedTreeHead& first_sth(observer_.sths[first_log_id]);
  EXPECT_EQ(21u, first_sth.tree_size);

  const std::string second_log_id("a\00d", 3);
  ASSERT_TRUE(observer_.sths.find(second_log_id) != observer_.sths.end());
}

// Does not notify of invalid STH JSON.
TEST_F(STHSetComponentInstallerTest, DoesNotLoadInvalidJSON) {
  const base::DictionaryValue manifest;
  const base::FilePath sths_dir(GetSTHsDir());
  CreateSTHsDir(manifest, sths_dir);

  const base::FilePath invalid_sth =
      sths_dir.Append(FILE_PATH_LITERAL("010101.sth"));
  WriteSTHToFile(std::string("{invalid json}"), invalid_sth);

  LoadSTHs(manifest, sths_dir);
  EXPECT_EQ(0u, observer_.sths.size());
}

// Does not notify of valid JSON but in a file not hex-encoded log id.
TEST_F(STHSetComponentInstallerTest,
       DoesNotLoadValidJSONFromFileNotHexEncoded) {
  const base::DictionaryValue manifest;
  const base::FilePath sths_dir(GetSTHsDir());
  CreateSTHsDir(manifest, sths_dir);

  const base::FilePath not_hex_sth_file =
      sths_dir.Append(FILE_PATH_LITERAL("nothex.sth"));
  WriteSTHToFile(net::ct::GetSampleSTHAsJson(), not_hex_sth_file);

  LoadSTHs(manifest, sths_dir);
  EXPECT_EQ(0u, observer_.sths.size());
}

// Tests recovery after network process crashes.
TEST_F(STHSetComponentInstallerTest, ReconfiguresAfterRestart) {
  const base::DictionaryValue manifest;
  const base::FilePath sths_dir(GetSTHsDir());
  CreateSTHsDir(manifest, sths_dir);

  const std::string good_sth_json = net::ct::GetSampleSTHAsJson();
  const base::FilePath sth_file =
      sths_dir.Append(FILE_PATH_LITERAL("616263.sth"));
  WriteSTHToFile(good_sth_json, sth_file);

  LoadSTHs(manifest, sths_dir);

  const std::string log_id("abc");
  ASSERT_TRUE(observer_.sths.find(log_id) != observer_.sths.end());
  const net::ct::SignedTreeHead& sth(observer_.sths[log_id]);
  EXPECT_EQ(21u, sth.tree_size);

  // Simulate a Network Service crash
  SimulateCrash();
  STHSetComponentInstallerPolicy::ReconfigureAfterNetworkRestart();
  thread_bundle_.RunUntilIdle();

  ASSERT_TRUE(observer_.sths.find(log_id) != observer_.sths.end());
}

}  // namespace component_updater
