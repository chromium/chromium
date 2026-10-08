// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_proto_conversion.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/file_upload_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "components/actor/core/actor_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data_fuzzable.pb.h"
#include "components/origin_gating/core/task_policy_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {

namespace {

using ::optimization_guide::proto::Action;
using ::optimization_guide::proto::AgentContainerConfig;
using ::optimization_guide::proto::Protocol;
using ::optimization_guide::proto::RuleMetadata;
using ::origin_gating::ClientTool;
using ::origin_gating::TaskPolicyConfig;

using Location = TaskPolicyConfig::Location;
using Rule = TaskPolicyConfig::Rule;
using Wildcard = TaskPolicyConfig::Wildcard;

Rule CreateExpectedRule(std::vector<Location> navigation_sources = {},
                        Rule::ResourceSet resources = {},
                        absl::flat_hash_set<ClientTool> allowed_tools = {}) {
  return Rule(std::move(navigation_sources), resources,
              std::move(allowed_tools));
}

optimization_guide::proto::LocationRule CreateRuleProto(
    std::vector<RuleMetadata::ActuationCapability> capabilities,
    std::vector<RuleMetadata::AgentResource> resources = {}) {
  optimization_guide::proto::LocationRule rule;
  rule.mutable_metadata()->mutable_capabilities()->Assign(capabilities.begin(),
                                                          capabilities.end());
  rule.mutable_metadata()->mutable_accessible_resources()->Assign(
      resources.begin(), resources.end());
  return rule;
}

optimization_guide::proto::LocationRule CreateWildcardRuleProto(
    std::vector<RuleMetadata::ActuationCapability> capabilities,
    std::vector<RuleMetadata::AgentResource> resources = {}) {
  optimization_guide::proto::LocationRule rule =
      CreateRuleProto(std::move(capabilities), std::move(resources));
  rule.mutable_location()->mutable_wildcard();
  return rule;
}

optimization_guide::proto::LocationRule CreateHttpsSiteRuleProto(
    std::string_view domain,
    std::vector<RuleMetadata::ActuationCapability> capabilities) {
  optimization_guide::proto::LocationRule rule =
      CreateRuleProto(std::move(capabilities));
  auto* site = rule.mutable_location()->mutable_site();
  site->set_protocol(Protocol::PROTOCOL_HTTPS);
  site->set_domain(std::string(domain));
  return rule;
}

AgentContainerConfig CreateConfigProto(
    std::vector<optimization_guide::proto::LocationRule> rules) {
  AgentContainerConfig config;
  config.mutable_location_rules()->Assign(rules.begin(), rules.end());
  return config;
}

optimization_guide::proto::Actions CreateActionsWithScriptTool(
    std::optional<int32_t> tab_id,
    std::optional<std::string> document_identifier) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ScriptToolAction* script_action =
      actions.add_actions()->mutable_script_tool();
  if (tab_id.has_value()) {
    script_action->set_tab_id(*tab_id);
  }
  script_action->set_tool_name("echo");
  script_action->set_input_arguments(R"({"text":"sample_input"})");
  if (document_identifier.has_value()) {
    script_action->mutable_document_identifier()->set_serialized_token(
        *document_identifier);
  }
  return actions;
}

// Returns an Actions proto holding a single ClickToUploadAction with a valid
// target but no files; tests add the files they need.
optimization_guide::proto::Actions CreateActionsWithClickToUpload(
    int32_t tab_id) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::ClickToUploadAction* upload_action =
      actions.add_actions()->mutable_click_to_upload();
  upload_action->set_tab_id(tab_id);
  optimization_guide::proto::ActionTarget* target =
      upload_action->mutable_target();
  target->set_content_node_id(42);
  target->mutable_document_identifier()->set_serialized_token(
      base::UnguessableToken::Create().ToString());
  return actions;
}

}  // namespace

class ActorProtoConversionTest : public testing::Test {
 public:
  ActorProtoConversionTest() {
    feature_list_.InitAndEnableFeature(kGlicActorEnableScriptTools);
  }
  ~ActorProtoConversionTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ActorProtoConversionTest, ConvertEmptyConfig) {
  AgentContainerConfig proto;
  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig({}, ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
}

TEST_F(ActorProtoConversionTest, ConvertWildcardRule) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_location()->mutable_wildcard();
  rule->mutable_metadata()->add_capabilities(RuleMetadata::CAPABILITY_ALL);
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig({{
                           {Location(Wildcard()),
                            CreateExpectedRule({}, {Rule::Resource::kSession},
                                               GetAllActorTools())},
                       }},
                       ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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
            TaskPolicyConfig(
                {{
                    {Location(net::SchemefulSite(GURL("https://example.com"))),
                     CreateExpectedRule({}, {Rule::Resource::kSession},
                                        GetAllActorTools())},
                }},
                ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig(
          {{
              {Location(url::Origin::Create(GURL("https://a.example.com"))),
               CreateExpectedRule({}, {Rule::Resource::kSession},
                                  GetAllActorTools())},
          }},
          ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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
            TaskPolicyConfig(
                {{
                    {Location(Wildcard()),
                     CreateExpectedRule({}, {Rule::Resource::kSession},
                                        GetAllActorTools())},
                    {Location(net::SchemefulSite(GURL("https://example.com"))),
                     CreateExpectedRule({}, {}, GetAllActorTools())},
                }},
                ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig({{
                           {Location(Wildcard()),
                            CreateExpectedRule({}, {Rule::Resource::kSession},
                                               GetAllActorTools())},
                       }},
                       ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
}

TEST_F(ActorProtoConversionTest, ConvertRuleWithNoCapabilities) {
  AgentContainerConfig proto;
  auto* rule = proto.add_location_rules();
  rule->mutable_location()->mutable_wildcard();
  rule->mutable_metadata()->add_accessible_resources(
      RuleMetadata::RESOURCE_SESSION);

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            TaskPolicyConfig(
                {{
                    {Location(Wildcard()),
                     CreateExpectedRule({}, {Rule::Resource::kSession}, {})},
                }},
                ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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
            TaskPolicyConfig(
                {{
                    {Location(net::SchemefulSite(GURL("http://http.com"))),
                     CreateExpectedRule({}, {}, GetAllActorTools())},
                    {Location(url::Origin::Create(GURL("ws://ws.com"))),
                     CreateExpectedRule({}, {}, GetAllActorTools())},
                    {Location(url::Origin::Create(GURL("wss://wss.com"))),
                     CreateExpectedRule({}, {}, GetAllActorTools())},
                }},
                ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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
      TaskPolicyConfig(
          {{
              {Location(Wildcard()),
               CreateExpectedRule(
                   {Location(url::Origin::Create(GURL("https://source.com")))},
                   {Rule::Resource::kSession}, GetAllActorTools())},
          }},
          ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig({{
                           {Location(Wildcard()),
                            CreateExpectedRule({}, {}, GetAllActorTools())},
                       }},
                       ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig({{
                           {Location(Wildcard()),
                            CreateExpectedRule({}, {}, GetAllActorTools())},
                       }},
                       ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig({{
                           {Location(Wildcard()),
                            CreateExpectedRule({}, {}, GetAllActorTools())},
                       }},
                       ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig({{
                           {Location(Wildcard()),
                            CreateExpectedRule({}, {}, GetAllActorTools())},
                       }},
                       ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
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

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig({{
                           {Location(Wildcard()),
                            CreateExpectedRule({}, {}, GetAllActorTools())},
                       }},
                       ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
}

TEST_F(ActorProtoConversionTest, ValidateActionsAreScriptTools_EmptyActions) {
  optimization_guide::proto::Actions actions;
  EXPECT_TRUE(ValidateActionsAreScriptTools(actions));
}

TEST_F(ActorProtoConversionTest,
       ValidateActionsAreScriptTools_OnlyScriptTools) {
  optimization_guide::proto::Actions actions;
  auto* action1 = actions.add_actions();
  action1->mutable_script_tool();
  auto* action2 = actions.add_actions();
  action2->mutable_script_tool();

  EXPECT_TRUE(ValidateActionsAreScriptTools(actions));
}

TEST_F(ActorProtoConversionTest,
       ValidateActionsAreScriptTools_NonScriptToolAction) {
  optimization_guide::proto::Actions actions;
  auto* action = actions.add_actions();
  action->mutable_wait();

  EXPECT_FALSE(ValidateActionsAreScriptTools(actions));
}

TEST_F(ActorProtoConversionTest, ValidateActionsAreScriptTools_MixedActions) {
  optimization_guide::proto::Actions actions;
  auto* action1 = actions.add_actions();
  action1->mutable_script_tool();
  auto* action2 = actions.add_actions();
  action2->mutable_click();

  EXPECT_FALSE(ValidateActionsAreScriptTools(actions));
}

TEST_F(
    ActorProtoConversionTest,
    BuildToolRequest_ScriptTool_ValidProto_CreatesRequestWithCorrectParameters) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  optimization_guide::proto::Actions actions =
      CreateActionsWithScriptTool(/*tab_id=*/100, token.ToString());

  BuildToolRequestResult requests = BuildToolRequest(actions);
  ASSERT_TRUE(requests.has_value());
  ASSERT_EQ(requests.value().size(), 1u);

  ToolRequest& created_request = *requests.value().front();
  EXPECT_EQ(ScriptToolRequest::kName, created_request.Name());

  const ScriptToolRequest& script_request =
      static_cast<const ScriptToolRequest&>(created_request);
  EXPECT_EQ(100, script_request.GetTabHandle().raw_value());
  EXPECT_EQ(token, script_request.GetTargetDocumentIdForTesting());
  EXPECT_EQ("echo", script_request.GetNameForTesting());
  EXPECT_EQ(R"({"text":"sample_input"})",
            script_request.GetInputArgumentsForTesting());
}

TEST_F(ActorProtoConversionTest,
       BuildToolRequest_ScriptTool_MissingDocumentIdentifier_ReturnsError) {
  optimization_guide::proto::Actions actions = CreateActionsWithScriptTool(
      /*tab_id=*/100, /*document_identifier=*/std::nullopt);

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kArgumentsInvalid)));
}

TEST_F(ActorProtoConversionTest,
       BuildToolRequest_ScriptTool_InvalidDocumentIdentifier_ReturnsError) {
  optimization_guide::proto::Actions actions =
      CreateActionsWithScriptTool(/*tab_id=*/100, "invalid_token");

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kArgumentsInvalid)));
}

TEST_F(ActorProtoConversionTest,
       BuildToolRequest_ScriptTool_MissingTabId_ReturnsError) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  optimization_guide::proto::Actions actions =
      CreateActionsWithScriptTool(/*tab_id=*/std::nullopt, token.ToString());

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kArgumentsInvalid)));
}

TEST_F(ActorProtoConversionTest,
       BuildToolRequest_ClickToUpload_CreatesRequestWithFiles) {
  base::test::ScopedFeatureList file_upload_feature;
  file_upload_feature.InitAndEnableFeature(kGlicActorFileUploadTool);

  optimization_guide::proto::Actions actions =
      CreateActionsWithClickToUpload(/*tab_id=*/100);
  optimization_guide::proto::ClickToUploadAction::File* file =
      actions.mutable_actions(0)->mutable_click_to_upload()->add_files();
  file->set_download_url("https://example.com/download?id=1");
  file->set_file_name("resume.pdf");

  BuildToolRequestResult requests = BuildToolRequest(actions);
  ASSERT_TRUE(requests.has_value());
  ASSERT_EQ(requests.value().size(), 1u);

  ToolRequest& created_request = *requests.value().front();
  EXPECT_EQ(FileUploadToolRequest::kName, created_request.Name());

  const FileUploadToolRequest& upload_request =
      static_cast<const FileUploadToolRequest&>(created_request);
  EXPECT_EQ(100, upload_request.GetTabHandle().raw_value());
  ASSERT_EQ(1u, upload_request.files().size());
  const FileUploadSource& source = upload_request.files().front();
  EXPECT_EQ(FileUploadSource::Type::kUrl, source.type);
  EXPECT_EQ(GURL("https://example.com/download?id=1"), source.url);
  EXPECT_EQ("resume.pdf", source.file_name);
}

TEST_F(ActorProtoConversionTest,
       BuildToolRequest_ClickToUpload_MissingDownloadUrl_ReturnsError) {
  base::test::ScopedFeatureList file_upload_feature;
  file_upload_feature.InitAndEnableFeature(kGlicActorFileUploadTool);

  optimization_guide::proto::Actions actions =
      CreateActionsWithClickToUpload(/*tab_id=*/100);
  // A file with only display metadata cannot be fetched.
  actions.mutable_actions(0)
      ->mutable_click_to_upload()
      ->add_files()
      ->set_file_name("resume.pdf");

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kArgumentsInvalid)));
}

TEST_F(ActorProtoConversionTest,
       BuildToolRequest_ClickToUpload_NoFiles_ReturnsError) {
  base::test::ScopedFeatureList file_upload_feature;
  file_upload_feature.InitAndEnableFeature(kGlicActorFileUploadTool);

  optimization_guide::proto::Actions actions =
      CreateActionsWithClickToUpload(/*tab_id=*/100);

  EXPECT_THAT(BuildToolRequest(actions),
              base::test::ErrorIs(testing::Pair(
                  0u, mojom::ActionResultCode::kFileUploadEmptyFileList)));
}

TEST_F(ActorProtoConversionTest, ConvertRuleWithSpecificCapabilities) {
  AgentContainerConfig proto = CreateConfigProto({CreateWildcardRuleProto(
      {RuleMetadata::CAPABILITY_CLICK, RuleMetadata::CAPABILITY_TYPE},
      {RuleMetadata::RESOURCE_SESSION})});

  EXPECT_EQ(
      ConvertAgentContainerConfig(proto),
      TaskPolicyConfig(
          {{
              {Location(Wildcard()),
               CreateExpectedRule({}, {Rule::Resource::kSession},
                                  {ClientTool(RuleMetadata::CAPABILITY_CLICK),
                                   ClientTool(RuleMetadata::CAPABILITY_TYPE)})},
          }},
          ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
}

TEST_F(ActorProtoConversionTest, ConvertRuleWithSpecificAndAllCapabilities) {
  AgentContainerConfig proto = CreateConfigProto({
      CreateWildcardRuleProto(
          {RuleMetadata::CAPABILITY_CLICK, RuleMetadata::CAPABILITY_ALL}),
      CreateHttpsSiteRuleProto("example.com", {RuleMetadata::CAPABILITY_ALL,
                                               RuleMetadata::CAPABILITY_CLICK}),
  });

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            TaskPolicyConfig(
                {{
                    {Location(Wildcard()),
                     CreateExpectedRule({}, {}, GetAllActorTools())},
                    {Location(net::SchemefulSite(GURL("https://example.com"))),
                     CreateExpectedRule({}, {}, GetAllActorTools())},
                }},
                ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
}

TEST_F(ActorProtoConversionTest,
       ConvertRuleWithSpecificAndUnknownCapabilities) {
  AgentContainerConfig proto = CreateConfigProto({CreateWildcardRuleProto(
      {RuleMetadata::CAPABILITY_UNKNOWN, RuleMetadata::CAPABILITY_CLICK})});

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            TaskPolicyConfig(
                {{
                    {Location(Wildcard()),
                     CreateExpectedRule(
                         {}, {}, {ClientTool(RuleMetadata::CAPABILITY_CLICK)})},
                }},
                ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
}

TEST_F(ActorProtoConversionTest, ConvertRuleWithOnlyUnknownCapability) {
  AgentContainerConfig proto = CreateConfigProto(
      {CreateWildcardRuleProto({RuleMetadata::CAPABILITY_UNKNOWN})});

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            TaskPolicyConfig(
                {{
                    {Location(Wildcard()), CreateExpectedRule({}, {}, {})},
                }},
                ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
}

TEST_F(ActorProtoConversionTest, ConvertRuleWithDuplicateCapabilities) {
  AgentContainerConfig proto = CreateConfigProto({CreateWildcardRuleProto(
      {RuleMetadata::CAPABILITY_CLICK, RuleMetadata::CAPABILITY_CLICK})});

  EXPECT_EQ(ConvertAgentContainerConfig(proto),
            TaskPolicyConfig(
                {{
                    {Location(Wildcard()),
                     CreateExpectedRule(
                         {}, {}, {ClientTool(RuleMetadata::CAPABILITY_CLICK)})},
                }},
                ClientTool(RuleMetadata::CAPABILITY_NAVIGATE)));
}

TEST_F(ActorProtoConversionTest, GetClientToolForRequest) {
  ClickToolRequest click_req(tabs::TabHandle(1), PageTarget(gfx::Point(0, 0)),
                             mojom::ClickType::kLeft,
                             mojom::ClickCount::kSingle);
  EXPECT_EQ(GetClientToolForRequest(click_req),
            ClientTool(RuleMetadata::CAPABILITY_CLICK));

  WaitToolRequest wait_req(base::Seconds(1), tabs::TabHandle(1));
  EXPECT_EQ(GetClientToolForRequest(wait_req),
            ClientTool(RuleMetadata::CAPABILITY_WAIT));

  HistoryBackToolRequest back_req(tabs::TabHandle(1));
  EXPECT_EQ(GetClientToolForRequest(back_req),
            ClientTool(RuleMetadata::CAPABILITY_BACK));

  HistoryForwardToolRequest forward_req(tabs::TabHandle(1));
  EXPECT_EQ(GetClientToolForRequest(forward_req),
            ClientTool(RuleMetadata::CAPABILITY_FORWARD));
}

TEST_F(ActorProtoConversionTest, GetClientToolForRequest_NoCapability) {
  ReloadPageToolRequest reload_req(tabs::TabHandle(1));
  EXPECT_EQ(GetClientToolForRequest(reload_req),
            ClientTool(RuleMetadata::CAPABILITY_UNKNOWN));
  EXPECT_FALSE(std::ranges::contains(
      GetAllActorTools(), ClientTool(RuleMetadata::CAPABILITY_UNKNOWN)));
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
