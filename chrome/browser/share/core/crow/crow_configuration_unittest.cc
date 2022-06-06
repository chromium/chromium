// Copyright 2022 The Chromium Authors. All rights reserved.
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

TEST_F(CrowConfigurationTest, Update) {
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
}

}  // namespace crow
