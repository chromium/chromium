// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_share_finder.h"

#include <algorithm>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/smb_client/discovery/in_memory_host_locator.h"
#include "chrome/browser/ash/smb_client/smb_constants.h"
#include "chrome/browser/ash/smb_client/smb_url.h"
#include "chromeos/ash/components/dbus/smbprovider/fake_smb_provider_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::smb_client {

namespace {

constexpr char kDefaultHost[] = "host";
constexpr char kDefaultAddress[] = "1.2.3.4";
constexpr char kDefaultUrl[] = "smb://host/";
constexpr char kDefaultResolvedUrl[] = "smb://1.2.3.4";

}  // namespace

class SmbShareFinderTest : public testing::Test {
 public:
  SmbShareFinderTest() {
    SetupShareFinderTest(true /* should_run_synchronously */);
  }
  SmbShareFinderTest(const SmbShareFinderTest&) = delete;
  SmbShareFinderTest& operator=(const SmbShareFinderTest&) = delete;
  ~SmbShareFinderTest() override = default;

 protected:
  void TearDown() override { fake_client_->ClearShares(); }

  // Adds host with |hostname| and |address| as the resolved url.
  void AddHost(const std::string& hostname, const std::string& address) {
    net::IPAddress ip_address;
    EXPECT_TRUE(ip_address.AssignFromIPLiteral(address));
    host_locator_->AddHost(hostname, ip_address);
  }

  // Adds the default host with the default address.
  void AddDefaultHost() { AddHost(kDefaultHost, kDefaultAddress); }

  // Adds |share| to the default host.
  void AddShareToDefaultHost(const std::string& share) {
    AddShare(kDefaultResolvedUrl, kDefaultUrl, share);
  }

  // Adds |share| a host. |resolved_url| will be in the format of
  // "smb://1.2.3.4" and |server_url| will be in the format of "smb://host/".
  void AddShare(const std::string& resolved_url,
                const std::string& server_url,
                const std::string& share) {
    fake_client_->AddToShares(resolved_url, share);
    expected_shares_.insert(server_url + share);
  }

  // Adds a share lookup failure for |resolved_url|.
  void AddShareFailure(const std::string& resolved_url,
                       smbprovider::ErrorType error) {
    fake_client_->AddGetSharesFailure(resolved_url, error);
  }

  // Helper function when expecting shares to be found in the network.
  void StartDiscoveryWhileExpectingSharesFound() {
    share_finder_->GatherSharesInNetwork(
        base::BindOnce(&SmbShareFinderTest::HostsDiscoveredCallback,
                       base::Unretained(this)),
        base::BindOnce(&SmbShareFinderTest::SharesFoundCallback,
                       base::Unretained(this)));
    EXPECT_TRUE(discovery_callback_called_);
  }

  // Helper function when expecting no shares to be found in the network.
  void ExpectNoSharesFound() {
    StartDiscoveryWhileExpectingEmptyShares();
    EXPECT_TRUE(discovery_callback_called_);
  }

  // Helper function to call SmbShareFinder::GatherSharesInNetwork. Asserts that
  // there are no shares discovered from the EmptySharesCallback.
  void StartDiscoveryWhileExpectingEmptyShares() {
    share_finder_->GatherSharesInNetwork(
        base::BindOnce(&SmbShareFinderTest::HostsDiscoveredCallback,
                       base::Unretained(this)),
        base::BindOnce(&SmbShareFinderTest::EmptySharesCallback,
                       base::Unretained(this)));
  }

  // Helper function to call SmbShareFinder::GatherSharesInNetwork. Asserts that
  // shares are found, but does not remove them.
  void StartDiscoveryWhileGatheringShares() {
    share_finder_->GatherSharesInNetwork(
        base::BindOnce(&SmbShareFinderTest::HostsDiscoveredCallback,
                       base::Unretained(this)),
        base::BindOnce(&SmbShareFinderTest::SharesFoundSizeCallback,
                       base::Unretained(this)));
  }

  // Helper function that expects expected_shares_ to be empty.
  void ExpectAllSharesHaveBeenFound() {
    EXPECT_TRUE(expected_shares_.empty());
    EXPECT_TRUE(share_discovery_callback_called_);
  }

  // Helper function that expects |url| to resolve to |expected|.
  void ExpectResolvedHost(const SmbUrl& url, const std::string& expected) {
    EXPECT_EQ(expected, share_finder_->GetResolvedUrl(url).ToString());
  }

  void ExpectDiscoveryCalled(int32_t expected) {
    EXPECT_EQ(expected, discovery_callback_counter_);
  }

  void FinishHostDiscoveryOnHostLocator() { host_locator_->RunCallback(); }

  void FinishShareDiscoveryOnSmbProviderClient() {
    fake_client_->RunStoredReadDirCallback();
  }

  void SetupShareFinderTest(bool should_run_synchronously) {
    auto host_locator =
        std::make_unique<InMemoryHostLocator>(should_run_synchronously);
    host_locator_ = host_locator.get();

    // If re-initializing the client, ensure the old client is destroyed first.
    fake_client_.reset();
    fake_client_ =
        std::make_unique<FakeSmbProviderClient>(should_run_synchronously);
    share_finder_ = std::make_unique<SmbShareFinder>(fake_client_.get());

    share_finder_->RegisterHostLocator(std::move(host_locator));
  }

 private:
  void HostsDiscoveredCallback() {
    discovery_callback_called_ = true;
    ++discovery_callback_counter_;
  }

  // Removes shares discovered from |expected_shares_|.
  void SharesFoundCallback(const std::vector<SmbUrl>& shares_found) {
    EXPECT_GE(shares_found.size(), 0u);

    for (const SmbUrl& url : shares_found) {
      EXPECT_EQ(1u, expected_shares_.erase(url.ToString()));
    }
    share_discovery_callback_called_ = true;
  }

  void SharesFoundSizeCallback(const std::vector<SmbUrl>& shares_found) {
    EXPECT_GE(shares_found.size(), 0u);
    share_discovery_callback_called_ = true;
  }

  void EmptySharesCallback(const std::vector<SmbUrl>& shares_found) {
    EXPECT_EQ(0u, shares_found.size());
    share_discovery_callback_called_ = true;
  }

  bool discovery_callback_called_ = false;
  bool share_discovery_callback_called_ = false;

  // Keeps track of expected shares across multiple hosts.
  std::set<std::string> expected_shares_;

  int32_t discovery_callback_counter_ = 0;

  raw_ptr<InMemoryHostLocator, DanglingUntriaged> host_locator_;
  std::unique_ptr<FakeSmbProviderClient> fake_client_;
  std::unique_ptr<SmbShareFinder> share_finder_;
};

TEST_F(SmbShareFinderTest, NoSharesFoundWithNoHosts) {
  ExpectNoSharesFound();
}

TEST_F(SmbShareFinderTest, NoSharesFoundWithEmptyHost) {
  AddDefaultHost();
  ExpectNoSharesFound();
}

TEST_F(SmbShareFinderTest, NoSharesFoundWithMultipleEmptyHosts) {
  AddDefaultHost();
  AddHost("host2", "4.5.6.7");
  ExpectNoSharesFound();
}

TEST_F(SmbShareFinderTest, SharesFoundWithSingleHost) {
  AddDefaultHost();
  AddShareToDefaultHost("share1");
  AddShareToDefaultHost("share2");

  StartDiscoveryWhileExpectingSharesFound();
  ExpectAllSharesHaveBeenFound();
}

TEST_F(SmbShareFinderTest, ErroWrithSingleHost) {
  AddDefaultHost();
  AddShareFailure(kDefaultResolvedUrl, smbprovider::ErrorType::ERROR_FAILED);

  StartDiscoveryWhileExpectingSharesFound();
  ExpectAllSharesHaveBeenFound();
}

TEST_F(SmbShareFinderTest, SharesFoundWithMultipleHosts) {
  AddDefaultHost();
  AddShareToDefaultHost("share1");

  const std::string host2 = "host2";
  const std::string address2 = "4.5.6.7";
  const std::string resolved_server_url2 = kSmbSchemePrefix + address2;
  const std::string server_url2 = kSmbSchemePrefix + host2 + "/";
  const std::string share2 = "share2";
  AddHost(host2, address2);
  AddShare(resolved_server_url2, server_url2, share2);

  StartDiscoveryWhileExpectingSharesFound();
  ExpectAllSharesHaveBeenFound();
}

TEST_F(SmbShareFinderTest, SharesFoundWithMultipleHostsAndLastFailed) {
  AddDefaultHost();
  AddShareToDefaultHost("share1");

  const std::string host2 = "host2";
  const std::string address2 = "4.5.6.7";
  const std::string resolved_server_url2 = kSmbSchemePrefix + address2;
  AddHost(host2, address2);
  // Note: This assumes that hosts will be queried for shares in lexicographical
  // order. This is currently true because SmbShareFinder receives hosts in an
  // ordered std::map<>.
  AddShareFailure(resolved_server_url2,
                  smbprovider::ErrorType::ERROR_SMB1_UNSUPPORTED);

  StartDiscoveryWhileExpectingSharesFound();
  ExpectAllSharesHaveBeenFound();
}

TEST_F(SmbShareFinderTest, SharesFoundOnOneHostWithMultipleHosts) {
  AddDefaultHost();
  AddShareToDefaultHost("share1");

  AddHost("host2", "4.5.6.7");

  StartDiscoveryWhileExpectingSharesFound();
  ExpectAllSharesHaveBeenFound();
}

TEST_F(SmbShareFinderTest, ResolvesHostToOriginalUrlIfNoHostFound) {
  const std::string url = std::string(kDefaultUrl) + "share";
  SmbUrl smb_url(url);

  // Trigger the NetworkScanner to scan the network with its HostLocators.
  StartDiscoveryWhileExpectingSharesFound();

  ExpectResolvedHost(smb_url, url);
}

TEST_F(SmbShareFinderTest, ResolvesHost) {
  AddDefaultHost();

  // Trigger the NetworkScanner to scan the network with its HostLocators.
  StartDiscoveryWhileExpectingSharesFound();

  SmbUrl url(std::string(kDefaultUrl) + "share");
  ExpectResolvedHost(url, std::string(kDefaultResolvedUrl) + "/share");
}

TEST_F(SmbShareFinderTest, ResolvesHostWithMultipleHosts) {
  AddDefaultHost();
  AddHost("host2", "4.5.6.7");

  // Trigger the NetworkScanner to scan the network with its HostLocators.
  StartDiscoveryWhileExpectingSharesFound();

  SmbUrl url("smb://host2/share");
  ExpectResolvedHost(url, "smb://4.5.6.7/share");
}

TEST_F(SmbShareFinderTest, TestNonEmptyDiscoveryWithNonEmptyShareCallback) {
  SetupShareFinderTest(false /* should_run_synchronoulsy */);

  AddDefaultHost();

  // Start discovery twice before host discovery is compeleted.
  StartDiscoveryWhileExpectingEmptyShares();
  StartDiscoveryWhileExpectingEmptyShares();

  // Assert discovery callback has not been called.
  ExpectDiscoveryCalled(0 /* expected */);

  FinishHostDiscoveryOnHostLocator();

  ExpectDiscoveryCalled(2 /* expected */);
}

TEST_F(SmbShareFinderTest, TestEmptyDiscoveryWithNonEmptyShareCallback) {
  SetupShareFinderTest(false /* should_run_synchronoulsy */);

  AddDefaultHost();
  AddShareToDefaultHost("share1");
  AddShareToDefaultHost("share2");

  // Makes call to start discovery once. Share discovery will not run and be in
  // a pending state.
  StartDiscoveryWhileGatheringShares();

  FinishHostDiscoveryOnHostLocator();

  ExpectDiscoveryCalled(1 /* expected */);

  // Host discovery will complete immediately while share discoveries will
  // remain pending.
  StartDiscoveryWhileExpectingSharesFound();

  ExpectDiscoveryCalled(2 /* expected */);

  // Run shares callback.
  FinishShareDiscoveryOnSmbProviderClient();

  ExpectAllSharesHaveBeenFound();
}

}  // namespace ash::smb_client
