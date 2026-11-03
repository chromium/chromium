// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/baguette_installer.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/hash.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {
namespace {

constexpr char kTestImageUrl[] = "https://example.com/baguette.img.zst";
constexpr std::string_view kTestImageData = "fake-baguette-image-contents";

class BaguetteInstallerTest : public testing::Test,
                              protected guest_os::FakeVmServicesHelper {
 protected:
  void SetUp() override {
    FakeDlcserviceClient()->set_install_root_path("/dlc/root/path");
    installer_ = std::make_unique<BaguetteInstaller>(
        &profile_, test_url_loader_factory_.GetSafeWeakWrapper());
  }

  void TearDown() override { installer_.reset(); }

  void SetValidImageDownloadResponse() {
    vm_tools::concierge::GetBaguetteImageUrlResponse url_response;
    url_response.set_url(kTestImageUrl);
    url_response.set_sha256(base::HexEncode(
        crypto::hash::Sha256(base::as_byte_span(kTestImageData))));
    FakeConciergeClient()->set_get_baguette_image_url_response(url_response);
    test_url_loader_factory_.AddResponse(kTestImageUrl, kTestImageData);
  }

  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestingProfile profile_;
  std::unique_ptr<BaguetteInstaller> installer_;
};

TEST_F(BaguetteInstallerTest, InstallFailsWhenConciergeUnavailable) {
  FakeConciergeClient()->set_wait_for_service_to_be_available_response(false);

  base::test::TestFuture<BaguetteInstaller::InstallResult,
                         std::optional<base::ScopedFD>>
      result_future;
  installer_->Install(result_future.GetCallback());

  EXPECT_EQ(result_future.Get<0>(), BaguetteInstaller::InstallResult::Failure);
  EXPECT_FALSE(result_future.Get<1>().has_value());
  EXPECT_EQ(FakeConciergeClient()->list_vm_disks_call_count(), 0);
}

TEST_F(BaguetteInstallerTest, InstallSkipsDownloadWhenBaguetteDiskExists) {
  vm_tools::concierge::ListVmDisksResponse list_response;
  list_response.set_success(true);
  auto* image = list_response.add_images();
  image->set_name(kCrostiniDefaultVmName);
  image->set_vm_type(
      vm_tools::concierge::VmInfo_VmType::VmInfo_VmType_BAGUETTE);
  FakeConciergeClient()->set_list_vm_disks_response(list_response);

  base::test::TestFuture<BaguetteInstaller::InstallResult,
                         std::optional<base::ScopedFD>>
      result_future;
  installer_->Install(result_future.GetCallback());

  EXPECT_EQ(result_future.Get<0>(), BaguetteInstaller::InstallResult::Success);
  EXPECT_FALSE(result_future.Get<1>().has_value());
  EXPECT_EQ(FakeConciergeClient()->get_baguette_image_url_call_count(), 0);
}

TEST_F(BaguetteInstallerTest, InstallDownloadsImageWhenNoBaguetteDiskExists) {
  SetValidImageDownloadResponse();

  base::test::TestFuture<BaguetteInstaller::InstallResult,
                         std::optional<base::ScopedFD>>
      result_future;
  installer_->Install(result_future.GetCallback());

  EXPECT_EQ(result_future.Get<0>(), BaguetteInstaller::InstallResult::Success);
  ASSERT_TRUE(result_future.Get<1>().has_value());
  EXPECT_TRUE(result_future.Get<1>()->is_valid());
  EXPECT_EQ(FakeConciergeClient()->get_baguette_image_url_call_count(), 1);
}

TEST_F(BaguetteInstallerTest,
       InstallFallsBackToDownloadWhenListVmDisksReturnsNullopt) {
  FakeConciergeClient()->set_list_vm_disks_response(std::nullopt);
  SetValidImageDownloadResponse();

  base::test::TestFuture<BaguetteInstaller::InstallResult,
                         std::optional<base::ScopedFD>>
      result_future;
  installer_->Install(result_future.GetCallback());

  EXPECT_EQ(result_future.Get<0>(), BaguetteInstaller::InstallResult::Success);
  ASSERT_TRUE(result_future.Get<1>().has_value());
  EXPECT_TRUE(result_future.Get<1>()->is_valid());
  EXPECT_EQ(FakeConciergeClient()->get_baguette_image_url_call_count(), 1);
}

TEST_F(BaguetteInstallerTest,
       InstallFailsCleanlyWhenBothListVmDisksAndGetUrlReturnNullopt) {
  FakeConciergeClient()->set_list_vm_disks_response(std::nullopt);
  FakeConciergeClient()->set_get_baguette_image_url_response(std::nullopt);

  base::test::TestFuture<BaguetteInstaller::InstallResult,
                         std::optional<base::ScopedFD>>
      result_future;
  installer_->Install(result_future.GetCallback());

  EXPECT_EQ(result_future.Get<0>(),
            BaguetteInstaller::InstallResult::DownloadError);
  EXPECT_FALSE(result_future.Get<1>().has_value());
}

TEST_F(BaguetteInstallerTest,
       InstallIgnoresImagesAndDownloadsWhenListVmDisksUnsuccessful) {
  vm_tools::concierge::ListVmDisksResponse list_response;
  list_response.set_success(false);
  auto* image = list_response.add_images();
  image->set_name(kCrostiniDefaultVmName);
  image->set_vm_type(
      vm_tools::concierge::VmInfo_VmType::VmInfo_VmType_BAGUETTE);
  FakeConciergeClient()->set_list_vm_disks_response(list_response);
  SetValidImageDownloadResponse();

  base::test::TestFuture<BaguetteInstaller::InstallResult,
                         std::optional<base::ScopedFD>>
      result_future;
  installer_->Install(result_future.GetCallback());

  EXPECT_EQ(result_future.Get<0>(), BaguetteInstaller::InstallResult::Success);
  ASSERT_TRUE(result_future.Get<1>().has_value());
  EXPECT_TRUE(result_future.Get<1>()->is_valid());
  EXPECT_EQ(FakeConciergeClient()->get_baguette_image_url_call_count(), 1);
}

}  // namespace
}  // namespace crostini
