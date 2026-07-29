// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_proto_conversion.h"

#include <optional>
#include <vector>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data_fuzzable.pb.h"
#include "components/origin_gating/core/actor_container_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {

namespace {

using ::optimization_guide::proto::AgentContainerConfig;
using ::optimization_guide::proto::Protocol;
using ::optimization_guide::proto::RuleMetadata;
using ::origin_gating::ActorContainerConfig;

using Location = ActorContainerConfig::Location;
using Rule = ActorContainerConfig::Rule;
using Wildcard = ActorContainerConfig::Wildcard;

Rule CreateExpectedRule(std::vector<Location> navigation_sources = {},
                        Rule::ResourceSet resources = {},
                        Rule::CapabilitySet capabilities = {}) {
  return Rule(std::move(navigation_sources), resources, capabilities);
}

}  // namespace

class ActorProtoConversionTest : public testing::Test {
 public:
  ActorProtoConversionTest() = default;
  ~ActorProtoConversionTest() override = default;
};

TEST_F(ActorProtoConversionTest, ConvertEmptyConfig) {
  AgentContainerConfig proto;
  EXPECT_EQ(ConvertAgentContainerConfig(proto), ActorContainerConfig());
}

TEST_F(ActorProtoConversionTest, ConvertWildcardRule) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_location()->mutable_wildcard();
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(Wildcard()),
                 CreateExpectedRule({}, {Rule::Resource::kSession},
                                    {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest, ConvertSiteRule) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  auto* site = rule->mutable_location()->mutable_site();
  site->set_protocol(Protocol::PROTOCOL_HTTPS);
  site->set_domain("example.com");
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(net::SchemefulSite(GURL("https://example.com"))),
                 CreateExpectedRule({}, {Rule::Resource::kSession},
                                    {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest, ConvertOriginRule) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  auto* origin = rule->mutable_location()->mutable_origin();
  origin->set_protocol(Protocol::PROTOCOL_HTTPS);
  origin->set_host("a.example.com");
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(url::Origin::Create(GURL("https://a.example.com"))),
                 CreateExpectedRule({}, {Rule::Resource::kSession},
                                    {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest, ConvertMultipleRules) {
  AgentContainerConfig proto;
  {
    auto* rule = proto.add_location_rules();
    rule->mutable_location()->mutable_wildcard();
    rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
    rule->mutable_metadata()->add_accessible_resources(
        RuleMetadata::RESOURCE_SESSION);
  }
  {
    auto* rule = proto.add_location_rules();
    auto* site = rule->mutable_location()->mutable_site();
    site->set_protocol(Protocol::PROTOCOL_HTTPS);
    site->set_domain("example.com");
    rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  }

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(Wildcard()),
                 CreateExpectedRule({}, {Rule::Resource::kSession},
                                    {Rule::Capability::kAll})},
                {Location(net::SchemefulSite(GURL("https://example.com"))),
                 CreateExpectedRule({}, {}, {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest, ConvertMixedResourcesAndCapabilities) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_location()->mutable_wildcard();
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_UNKNOWN);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_UNKNOWN);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(Wildcard()),
                 CreateExpectedRule({}, {Rule::Resource::kSession},
                                    {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest, ConvertRuleWithNoCapabilities) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_location()->mutable_wildcard();
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(Wildcard()),
                 CreateExpectedRule({}, {Rule::Resource::kSession}, {})},
            }}));
}

TEST_F(ActorProtoConversionTest, ConvertProtocols_Http_Ws_Wss) {
  AgentContainerConfig proto;
  {
    auto* rule = proto.add_location_rules();
    auto* site = rule->mutable_location()->mutable_site();
    site->set_protocol(Protocol::PROTOCOL_HTTP);
    site->set_domain("http.com");
    rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  }
  {
    auto* rule = proto.add_location_rules();
    auto* origin = rule->mutable_location()->mutable_origin();
    origin->set_protocol(Protocol::PROTOCOL_WS);
    origin->set_host("ws.com");
    rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  }
  {
    auto* rule = proto.add_location_rules();
    auto* origin = rule->mutable_location()->mutable_origin();
    origin->set_protocol(Protocol::PROTOCOL_WSS);
    origin->set_host("wss.com");
    rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  }

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(net::SchemefulSite(GURL("http://http.com"))),
                 CreateExpectedRule({}, {}, {Rule::Capability::kAll})},
                {Location(url::Origin::Create(GURL("ws://ws.com"))),
                 CreateExpectedRule({}, {}, {Rule::Capability::kAll})},
                {Location(url::Origin::Create(GURL("wss://wss.com"))),
                 CreateExpectedRule({}, {}, {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest, ConvertWithNavigationSources) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_location()->mutable_wildcard();
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  auto* nav_source = rule->add_navigation_sources();
  auto* source_origin = nav_source->mutable_source()->mutable_origin();
  source_origin->set_protocol(Protocol::PROTOCOL_HTTPS);
  source_origin->set_host("source.com");

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      ActorContainerConfig({{
          {Location(Wildcard()),
           CreateExpectedRule(
               {Location(url::Origin::Create(GURL("https://source.com")))},
               {Rule::Resource::kSession}, {Rule::Capability::kAll})},
      }}));
}

TEST_F(ActorProtoConversionTest, FiltersOutMalformedRules_SiteUnknownProtocol) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  auto* site = rule->mutable_location()->mutable_site();
  site->set_domain("example.com");
  site->set_protocol(Protocol::PROTOCOL_UNKNOWN);

  auto* valid_rule = proto.add_location_rules();
  valid_rule->mutable_location()->mutable_wildcard();
  valid_rule->mutable_metadata()->add_capabilities(
      RuleMetadata::CAPABILITY_ALL);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(Wildcard()),
                 CreateExpectedRule({}, {}, {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest,
       FiltersOutMalformedRules_OriginUnknownProtocol) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  auto* origin = rule->mutable_location()->mutable_origin();
  origin->set_host("a.example.com");
  origin->set_protocol(Protocol::PROTOCOL_UNKNOWN);

  auto* valid_rule = proto.add_location_rules();
  valid_rule->mutable_location()->mutable_wildcard();
  valid_rule->mutable_metadata()->add_capabilities(
      RuleMetadata::CAPABILITY_ALL);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(Wildcard()),
                 CreateExpectedRule({}, {}, {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest, FiltersOutMalformedRules_SiteNoDomain) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  auto* site = rule->mutable_location()->mutable_site();
  site->set_protocol(Protocol::PROTOCOL_HTTPS);

  auto* valid_rule = proto.add_location_rules();
  valid_rule->mutable_location()->mutable_wildcard();
  valid_rule->mutable_metadata()->add_capabilities(
      RuleMetadata::CAPABILITY_ALL);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(Wildcard()),
                 CreateExpectedRule({}, {}, {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest, FiltersOutMalformedRules_EmptyLocationRule) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  auto* valid_rule = proto.add_location_rules();
  valid_rule->mutable_location()->mutable_wildcard();
  valid_rule->mutable_metadata()->add_capabilities(
      RuleMetadata::CAPABILITY_ALL);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(Wildcard()),
                 CreateExpectedRule({}, {}, {Rule::Capability::kAll})},
            }}));
}

TEST_F(ActorProtoConversionTest,
       FiltersOutMalformedRules_SiteEmptyNavigationSource) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  auto* site = rule->mutable_location()->mutable_site();
  site->set_domain("example.com");
  site->set_protocol(Protocol::PROTOCOL_HTTPS);

  rule->add_navigation_sources();

  auto* valid_rule = proto.add_location_rules();
  valid_rule->mutable_location()->mutable_wildcard();
  valid_rule->mutable_metadata()->add_capabilities(
      RuleMetadata::CAPABILITY_ALL);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            ActorContainerConfig({{
                {Location(Wildcard()),
                 CreateExpectedRule({}, {}, {Rule::Capability::kAll})},
            }}));
}

void CanConvertAnyProto(
    const fuzzable::optimization_guide::proto::AgentContainerConfig&
        fuzzable_config_proto) {
  std::string serialized;
  CHECK(fuzzable_config_proto.SerializeToString(&serialized));
  optimization_guide::proto::AgentContainerConfig config_proto;
  CHECK(config_proto.ParseFromString(serialized));

  ConvertAgentContainerConfig(config_proto);
}

FUZZ_TEST(ActorProtoConversionFuzzTest, CanConvertAnyProto);

}  // namespace actor
