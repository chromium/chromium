// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/core/crow/crow_configuration.h"

#include "chrome/browser/share/proto/crow_configuration.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;

namespace crow {

class MockCrowConfiguration : public CrowConfiguration {
 public:
  MockCrowConfiguration() = default;
  ~MockCrowConfiguration() override = default;
};

class CrowConfigurationTest : public testing::Test {
 protected:
  CrowConfigurationTest() = default;
  ~CrowConfigurationTest() override = default;

 protected:
  NiceMock<MockCrowConfiguration> config_;
};

TEST_F(CrowConfigurationTest, GetPublicationIDBeforeUpdate) {
  // Calls to GetPublicatoinID before data arrives simply return 'no.'
  EXPECT_EQ(config_.GetPublicationIDFromAllowlist("pub2.tld"), "");
}

TEST_F(CrowConfigurationTest, UpdateAllowlist) {
  crow::mojom::CrowConfiguration proto;
  ::crow::mojom::Publisher* publisher1 = proto.add_publisher();
  publisher1->set_publication_id("Pub1");
  publisher1->add_host("pub1.tld");
  publisher1->add_host("blog.pub1.tld");

  ::crow::mojom::Publisher* publisher2 = proto.add_publisher();
  publisher2->set_publication_id("Pub2");
  publisher2->add_host("pub2.tld");

  config_.PopulateFromBinaryPb(proto.SerializeAsString());

  EXPECT_EQ(config_.domains_.size(), 3u);
  EXPECT_EQ(config_.domains_["pub1.tld"], "Pub1");
  EXPECT_EQ(config_.domains_["blog.pub1.tld"], "Pub1");
  EXPECT_EQ(config_.domains_["pub2.tld"], "Pub2");
  EXPECT_EQ(config_.GetPublicationIDFromAllowlist("pub1.tld"), "Pub1");
  EXPECT_EQ(config_.GetPublicationIDFromAllowlist("blog.pub1.tld"), "Pub1");
  EXPECT_EQ(config_.GetPublicationIDFromAllowlist("pub2.tld"), "Pub2");
}

TEST_F(CrowConfigurationTest, UpdateWithDenylist) {
  // Both allowlist and denylist are populated; allowlist should
  // not have an effect on denylist checks though a host overlaps.
  crow::mojom::CrowConfiguration proto;
  ::crow::mojom::Publisher* publisher1 = proto.add_publisher();
  publisher1->set_publication_id("Pub1");
  publisher1->add_host("pub1.tld");
  publisher1->add_host("blog.pub1.tld");

  ::crow::mojom::Publisher* publisher2 = proto.add_publisher();
  publisher2->set_publication_id("Pub2");
  publisher2->add_host("pub2.tld");

  proto.add_denied_hosts("pub2.tld");
  proto.add_denied_hosts("getrichquick.scam");

  config_.PopulateFromBinaryPb(proto.SerializeAsString());

  EXPECT_FALSE(config_.DenylistContainsHost("pub1.tld"));
  EXPECT_TRUE(config_.DenylistContainsHost("pub2.tld"));
  EXPECT_TRUE(config_.DenylistContainsHost("getrichquick.scam"));
}

}  // namespace crow
