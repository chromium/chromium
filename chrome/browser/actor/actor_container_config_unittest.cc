// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_container_config.h"

#include "base/test/gtest_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

namespace {

optimization_guide::proto::LocationRule CreateWildcardLocationRule() {
  optimization_guide::proto::LocationRule rule;
  rule.mutable_location()->mutable_wildcard();
  return rule;
}

optimization_guide::proto::LocationRule CreateSiteLocationRule(
    std::string domain,
    optimization_guide::proto::Protocol protocol =
        optimization_guide::proto::Protocol::PROTOCOL_HTTPS) {
  optimization_guide::proto::LocationRule rule;
  optimization_guide::proto::Site* site =
      rule.mutable_location()->mutable_site();
  CHECK(site);
  site->set_protocol(protocol);
  site->set_domain(domain);
  return rule;
}

optimization_guide::proto::LocationRule CreateOriginLocationRule(
    std::string host,
    optimization_guide::proto::Protocol protocol =
        optimization_guide::proto::Protocol::PROTOCOL_HTTPS,
    int port = 0) {
  optimization_guide::proto::LocationRule rule;
  optimization_guide::proto::Origin* origin =
      rule.mutable_location()->mutable_origin();
  CHECK(origin);
  origin->set_protocol(protocol);
  origin->set_host(host);
  if (port > 0) {
    origin->set_port(port);
  }
  return rule;
}

optimization_guide::proto::NavigationSource CreateOriginNavigationSource(
    std::string host,
    optimization_guide::proto::Protocol protocol =
        optimization_guide::proto::Protocol::PROTOCOL_HTTPS,
    int port = 443) {
  optimization_guide::proto::NavigationSource nav_source;
  optimization_guide::proto::Origin* origin =
      nav_source.mutable_source()->mutable_origin();
  origin->set_protocol(protocol);
  origin->set_host(host);
  origin->set_port(port);
  return nav_source;
}

}  // namespace

class ActorContainerConfigTest : public testing::Test {
 public:
  ~ActorContainerConfigTest() override = default;

  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin kExampleDifferentSubdomainOrigin =
      url::Origin::Create(GURL("https://b.example.com"));
  const url::Origin kExampleInsecureOrigin =
      url::Origin::Create(GURL("http://a.example.com"));
  const url::Origin kCrossSiteOrigin =
      url::Origin::Create(GURL("https://b.foo.com"));

  const url::Origin kIgnoredOrigin =
      url::Origin::Create(GURL("https://ignoreme.com"));

  const url::Origin kWsOrigin = url::Origin::Create(GURL("ws://a.example.com"));
  const url::Origin kWssOrigin =
      url::Origin::Create(GURL("wss://a.example.com"));
  const url::Origin kCrossSiteWsOrigin =
      url::Origin::Create(GURL("ws://b.foo.com"));
  const url::Origin kCrossSiteWssOrigin =
      url::Origin::Create(GURL("wss://b.foo.com"));
};

TEST_F(ActorContainerConfigTest, Assign_ValidAssignment) {
  ActorContainerConfig config;
  EXPECT_FALSE(config.IsActive());

  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config_with_proto(config_proto);
  EXPECT_TRUE(config_with_proto.IsActive());

  config.Assign(config_with_proto);
  EXPECT_TRUE(config.IsActive());
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Assign_EmptyConfig) {
  ActorContainerConfig config;
  EXPECT_FALSE(config.IsActive());

  ActorContainerConfig other_empty_config;
  config.Assign(other_empty_config);
  EXPECT_FALSE(config.IsActive());
  EXPECT_CHECK_DEATH(
      config.IsNavigationAllowed(kExampleOrigin, kIgnoredOrigin));
}

TEST_F(ActorContainerConfigTest, Assign_EmptyConfig_IgnoresSecondCall) {
  ActorContainerConfig config;
  EXPECT_FALSE(config.IsActive());

  ActorContainerConfig other_empty_config;
  config.Assign(other_empty_config);
  EXPECT_FALSE(config.IsActive());

  // Should ignore config after first call to Assign.
  optimization_guide::proto::AgentContainerConfig new_config_proto;
  *new_config_proto.add_location_rules() =
      CreateSiteLocationRule("ignoreme.com");
  new_config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_capabilities(
          optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  new_config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config_with_different_proto(new_config_proto);
  EXPECT_TRUE(config_with_different_proto.IsActive());
  EXPECT_TRUE(config_with_different_proto.IsActuationAllowed(kIgnoredOrigin));
  EXPECT_TRUE(config_with_different_proto.IsNavigationAllowed(kExampleOrigin,
                                                              kIgnoredOrigin));

  // Calling Assign again should be allowed, but it should not grant navigation
  // to the new site the second invocation.
  config.Assign(config_with_different_proto);
  EXPECT_FALSE(config.IsActive());
  EXPECT_CHECK_DEATH(config.IsActuationAllowed(kIgnoredOrigin));
  EXPECT_CHECK_DEATH(
      config.IsNavigationAllowed(kExampleOrigin, kIgnoredOrigin));
}

TEST_F(ActorContainerConfigTest, Assign_IgnoresSecondCall) {
  ActorContainerConfig config;
  EXPECT_FALSE(config.IsActive());

  // First config already has a config set.
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  ActorContainerConfig already_set_config(config_proto);
  EXPECT_TRUE(already_set_config.IsActive());

  // Start by calling Assign, below we test the next call is ignored.
  config.Assign(already_set_config);
  EXPECT_TRUE(config.IsActive());

  // Should ignore config after first call to Assign.
  optimization_guide::proto::AgentContainerConfig new_config_proto;
  *new_config_proto.add_location_rules() =
      CreateSiteLocationRule("ignoreme.com");
  new_config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_capabilities(
          optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  new_config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config_with_different_proto(new_config_proto);
  EXPECT_TRUE(config_with_different_proto.IsActive());
  EXPECT_TRUE(config_with_different_proto.IsActuationAllowed(kIgnoredOrigin));
  EXPECT_TRUE(config_with_different_proto.IsNavigationAllowed(kExampleOrigin,
                                                              kIgnoredOrigin));

  // Calling Assign again should be allowed, but it should not grant navigation
  // to the new site the second invocation.
  config.Assign(config_with_different_proto);
  EXPECT_TRUE(config.IsActive());
  EXPECT_FALSE(config.IsActuationAllowed(kIgnoredOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kIgnoredOrigin));

  // Calling Assign on a config that is already set should be a no-op.
  already_set_config.Assign(config_with_different_proto);
  EXPECT_FALSE(already_set_config.IsActuationAllowed(kIgnoredOrigin));
  EXPECT_FALSE(
      already_set_config.IsNavigationAllowed(kExampleOrigin, kIgnoredOrigin));
}

TEST_F(ActorContainerConfigTest, IsActive) {
  ActorContainerConfig config_with_no_proto;
  EXPECT_FALSE(config_with_no_proto.IsActive());

  optimization_guide::proto::AgentContainerConfig config_proto;
  ActorContainerConfig config_with_proto(config_proto);
  EXPECT_TRUE(config_with_proto.IsActive());
}

TEST_F(ActorContainerConfigTest, EmptyConfigCannotUse) {
  ActorContainerConfig config;
  EXPECT_FALSE(config.IsActive());
  EXPECT_CHECK_DEATH(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_CHECK_DEATH(
      config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, EmptyProtoBlocksAll) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, NoCapabilities) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateWildcardLocationRule();
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Wildcard_ActuationCapabilityAll) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateWildcardLocationRule();
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_TRUE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Wildcard_WithSource) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateWildcardLocationRule();
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  *config_proto.mutable_location_rules(0)->add_navigation_sources() =
      CreateOriginNavigationSource("a.example.com");
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_TRUE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Site_NoCapabilities) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Site_ActuationCapabilityAll) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  // First is not allowed since the rule only permits navigation to secure URLs
  // at example.com, but second is allowed because it does not forbid navigation
  // from insecure URLs.
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, InsecureSite_ActuationCapabilityAll) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule(
      "example.com", optimization_guide::proto::Protocol::PROTOCOL_HTTP);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  // Only navigation to the insecure URL is allowed.
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Site_WithSource) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  *config_proto.mutable_location_rules(0)->add_navigation_sources() =
      CreateOriginNavigationSource("a.example.com");
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Origin_NoCapabilities) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() =
      CreateOriginLocationRule("a.example.com");
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Origin_ActuationCapabilityAll) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() =
      CreateOriginLocationRule("a.example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, InsecureOrigin_ActuationCapabilityAll) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateOriginLocationRule(
      "a.example.com", optimization_guide::proto::Protocol::PROTOCOL_HTTP);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest,
       OriginWithExplicitPort_ActuationCapabilityAll) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateOriginLocationRule(
      "a.example.com", optimization_guide::proto::Protocol::PROTOCOL_HTTPS,
      /*port=*/443);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, Origin_WithSource) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() =
      CreateOriginLocationRule("a.example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  *config_proto.mutable_location_rules(0)->add_navigation_sources() =
      CreateOriginNavigationSource("a.example.com");
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, WildcardAndBlockedSite) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateWildcardLocationRule();
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                          kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_TRUE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, BlockedWildcardAndAllowedSite) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateWildcardLocationRule();
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(1)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(1)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin,
                                         kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, SiteAndBlockedOrigin) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  *config_proto.add_location_rules() =
      CreateOriginLocationRule("b.example.com");
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));
  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, BlockedSiteAndOrigin) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  *config_proto.add_location_rules() =
      CreateOriginLocationRule("a.example.com");
  config_proto.mutable_location_rules(1)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(1)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Same-site.
  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Same-site, different hosts.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleDifferentSubdomainOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleDifferentSubdomainOrigin,
                                         kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin,
                                          kExampleDifferentSubdomainOrigin));

  // Cross-scheme.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleInsecureOrigin));
  EXPECT_FALSE(
      config.IsNavigationAllowed(kExampleOrigin, kExampleInsecureOrigin));
  EXPECT_TRUE(
      config.IsNavigationAllowed(kExampleInsecureOrigin, kExampleOrigin));

  // Cross-site.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kCrossSiteOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kCrossSiteOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, NoCapability) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, CapabilityUnknown) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_UNKNOWN);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, MultipleCapabilityUnknown) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_UNKNOWN);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_UNKNOWN);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, MultipleCapabilityAll) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, MixedCapabilitiesUnknownAndAll) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_UNKNOWN);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, NoResources) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, ResourceUnknown) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_UNKNOWN);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, MultipleResourceUnknowns) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_UNKNOWN);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_UNKNOWN);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, MixedResourcesUnknownAndSession) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_UNKNOWN);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, MultipleResourceSessions) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_TRUE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, SiteWithUnknownProtocol) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule(
      "example.com", optimization_guide::proto::Protocol::PROTOCOL_UNKNOWN);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, OriginWithUnknownProtocol) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateOriginLocationRule(
      "a.example.com", optimization_guide::proto::Protocol::PROTOCOL_UNKNOWN);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, SiteWithNoDomain) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)
      ->mutable_location()
      ->mutable_site()
      ->clear_domain();
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, OriginWithNoHost) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() =
      CreateOriginLocationRule("a.example.com");
  config_proto.mutable_location_rules(0)
      ->mutable_location()
      ->mutable_origin()
      ->clear_host();
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, EmptyLocationRule) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  config_proto.add_location_rules();
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, SiteWithEmptySource) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateSiteLocationRule("example.com");
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  config_proto.mutable_location_rules(0)->add_navigation_sources();
  ActorContainerConfig config(config_proto);

  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));
}

TEST_F(ActorContainerConfigTest, WsOrigin) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateOriginLocationRule(
      "a.example.com", optimization_guide::proto::Protocol::PROTOCOL_WS);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Should not match https://.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Can only navigate https:// -> ws://.
  EXPECT_TRUE(config.IsActuationAllowed(kWsOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kWsOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kExampleOrigin));

  // Only navigation from wss:// -> ws:// allowed.
  EXPECT_FALSE(config.IsActuationAllowed(kWssOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kWssOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kWssOrigin, kWsOrigin));

  // Navigate to ws:// with different host should not be allowed.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteWsOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWsOrigin, kCrossSiteWsOrigin));
}

TEST_F(ActorContainerConfigTest, WssOrigin) {
  optimization_guide::proto::AgentContainerConfig config_proto;
  *config_proto.add_location_rules() = CreateOriginLocationRule(
      "a.example.com", optimization_guide::proto::Protocol::PROTOCOL_WSS);
  config_proto.mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config_proto.mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  ActorContainerConfig config(config_proto);

  // Should not match https://.
  EXPECT_FALSE(config.IsActuationAllowed(kExampleOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kExampleOrigin, kExampleOrigin));

  // Can only navigate https:// -> wss://.
  EXPECT_FALSE(config.IsActuationAllowed(kWsOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kExampleOrigin, kWssOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWssOrigin, kExampleOrigin));

  // Only navigation from ws:// -> wss:// allowed.
  EXPECT_TRUE(config.IsActuationAllowed(kWssOrigin));
  EXPECT_TRUE(config.IsNavigationAllowed(kWsOrigin, kWssOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWssOrigin, kWsOrigin));

  // Navigate to wss:// with different host should not be allowed.
  EXPECT_FALSE(config.IsActuationAllowed(kCrossSiteWsOrigin));
  EXPECT_FALSE(config.IsNavigationAllowed(kWssOrigin, kCrossSiteWssOrigin));
}

}  // namespace actor
