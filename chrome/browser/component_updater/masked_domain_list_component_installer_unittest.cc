// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/masked_domain_list_component_installer.h"

#include <string>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#include "net/base/features.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
using ::testing::_;

constexpr char kMaskedDomainListProto[] = "masked_domain_list.MaskedDomainList";
constexpr char kUpdateSuccessHistogram[] =
    "NetworkService.IpProtection.ProxyAllowList.UpdateSuccess";
constexpr char kUpdateProcessTimeHistogram[] =
    "NetworkService.IpProtection.ProxyAllowList.UpdateProcessTime";
constexpr char kFlatbufferBuildTimeHistogram[] =
    "NetworkService.IpProtection.ProxyAllowList.FlatbufferBuildTime";
constexpr char kMdlSizeHistogram[] = "NetworkService.MaskedDomainList.Size2";
constexpr char kDiskUsageHistogram[] =
    "NetworkService.MaskedDomainList.DiskUsage";

}  // namespace

class MaskedDomainListComponentInstallerTest : public ::testing::Test {
 public:
  MaskedDomainListComponentInstallerTest() {
    content::GetNetworkService();  // Initializes Network Service.
  }

  mojo_base::ProtoWrapper FakeMdl() {
    masked_domain_list::MaskedDomainList mdl;
    masked_domain_list::ResourceOwner* resource_owner =
        mdl.add_resource_owners();
    resource_owner->set_owner_name("foo");
    resource_owner->add_owned_properties("property.example.com");
    resource_owner->add_owned_resources()->set_domain("resource.example.com");

    std::string proto_str;
    EXPECT_TRUE(mdl.SerializeToString(&proto_str));
    return mojo_base::ProtoWrapper(base::as_byte_span(proto_str),
                                   kMaskedDomainListProto,
                                   mojo_base::ProtoWrapperBytes::GetPassKey());
  }

 protected:
  // Ensure each test run has its own user-data directory.
  const base::ScopedPathOverride path_override{chrome::DIR_USER_DATA};

  content::BrowserTaskEnvironment env_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

TEST_F(MaskedDomainListComponentInstallerTest, FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      network::features::kMaskedDomainList);
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  EXPECT_CALL(*service, RegisterComponent(_)).Times(0);
  RegisterMaskedDomainListComponent(service.get());

  env_.RunUntilIdle();
}

TEST_F(MaskedDomainListComponentInstallerTest, FeatureEnabled_NoFileExists) {
  scoped_feature_list_.InitWithFeatures(
      {network::features::kMaskedDomainList,
       net::features::kEnableIpProtectionProxy},
      {});
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(_)).Times(1);
  RegisterMaskedDomainListComponent(service.get());
  env_.RunUntilIdle();

  // If no file has been passed, the allow list is not populated.
  EXPECT_FALSE(network::NetworkService::GetNetworkServiceForTesting()
                   ->masked_domain_list_manager()
                   ->IsPopulated());
}

TEST_F(MaskedDomainListComponentInstallerTest, OnMaskedDomainListReadyProto) {
  scoped_feature_list_.InitWithFeatures(
      {network::features::kMaskedDomainList},
      {network::features::kMaskedDomainListFlatbufferImpl});

  OnMaskedDomainListReady(base::Version(), FakeMdl());
  EXPECT_TRUE(base::test::RunUntil([&] {
    return network::NetworkService::GetNetworkServiceForTesting()
        ->masked_domain_list_manager()
        ->IsPopulated();
  }));
  histogram_tester_.ExpectTotalCount(kUpdateSuccessHistogram, 1);
  histogram_tester_.ExpectTotalCount(kUpdateProcessTimeHistogram, 1);
  histogram_tester_.ExpectTotalCount(kFlatbufferBuildTimeHistogram, 0);
  histogram_tester_.ExpectTotalCount(kMdlSizeHistogram, 1);
  histogram_tester_.ExpectTotalCount(kDiskUsageHistogram, 0);
}

TEST_F(MaskedDomainListComponentInstallerTest,
       OnMaskedDomainListReadyFlatbuffer) {
  scoped_feature_list_.InitWithFeatures(
      {network::features::kMaskedDomainList,
       network::features::kMaskedDomainListFlatbufferImpl},
      {});

  OnMaskedDomainListReady(base::Version(), FakeMdl());
  EXPECT_TRUE(base::test::RunUntil([&] {
    return network::NetworkService::GetNetworkServiceForTesting()
        ->masked_domain_list_manager()
        ->IsPopulated();
  }));
  histogram_tester_.ExpectTotalCount(kUpdateSuccessHistogram, 1);
  histogram_tester_.ExpectTotalCount(kUpdateProcessTimeHistogram, 0);
  histogram_tester_.ExpectTotalCount(kFlatbufferBuildTimeHistogram, 1);
  histogram_tester_.ExpectTotalCount(kMdlSizeHistogram, 1);
  // Both MDLs get measured, so two records.
  histogram_tester_.ExpectTotalCount(kDiskUsageHistogram, 2);
}

}  // namespace component_updater
