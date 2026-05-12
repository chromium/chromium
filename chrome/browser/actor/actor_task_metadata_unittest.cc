// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task_metadata.h"

#include "base/test/protobuf_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {

class ActorTaskMetadataTest : public testing::Test {
 public:
  ~ActorTaskMetadataTest() override = default;
};

TEST_F(ActorTaskMetadataTest, AddedWritableMainframeOrigins) {
  // Empty ActorTaskMetadata.
  {
    ActorTaskMetadata metadata;
    EXPECT_EQ(0u, metadata.added_writable_mainframe_origins().size());
  }

  // Actions proto does not have TaskMetadata.
  {
    optimization_guide::proto::Actions actions;
    ActorTaskMetadata metadata(actions);
    EXPECT_EQ(0u, metadata.added_writable_mainframe_origins().size());
  }

  // Actions proto does not have SecurityMetadata.
  {
    optimization_guide::proto::Actions actions;
    optimization_guide::proto::TaskMetadata* task_metadata =
        actions.mutable_task_metadata();
    EXPECT_TRUE(task_metadata);
    ActorTaskMetadata metadata(actions);
    EXPECT_EQ(0u, metadata.added_writable_mainframe_origins().size());
  }

  // Actions has SecurityMetadata with no added origins.
  {
    optimization_guide::proto::Actions actions;
    optimization_guide::proto::TaskMetadata* task_metadata =
        actions.mutable_task_metadata();
    EXPECT_TRUE(task_metadata);
    optimization_guide::proto::SecurityMetadata* security =
        task_metadata->mutable_security();
    EXPECT_TRUE(security);
    ActorTaskMetadata metadata(actions);
    EXPECT_EQ(0u, metadata.added_writable_mainframe_origins().size());
  }

  // Actions adds writable mainframe origins.
  {
    optimization_guide::proto::Actions actions;
    optimization_guide::proto::TaskMetadata* task_metadata =
        actions.mutable_task_metadata();
    EXPECT_TRUE(task_metadata);
    optimization_guide::proto::SecurityMetadata* security =
        task_metadata->mutable_security();
    EXPECT_TRUE(security);
    security->add_added_writable_mainframe_origins("https://a.foo.com");
    security->add_added_writable_mainframe_origins("https://b.bar.com");
    ActorTaskMetadata metadata(actions);
    EXPECT_THAT(metadata.added_writable_mainframe_origins(),
                testing::UnorderedElementsAre(
                    url::Origin::Create(GURL("https://a.foo.com")),
                    url::Origin::Create(GURL("https://b.bar.com"))));
  }
}

TEST_F(ActorTaskMetadataTest, AgentContainerConfig) {
  optimization_guide::proto::Actions actions;
  optimization_guide::proto::TaskMetadata* task_metadata =
      actions.mutable_task_metadata();
  EXPECT_TRUE(task_metadata);
  optimization_guide::proto::SecurityMetadata* security =
      task_metadata->mutable_security();
  EXPECT_TRUE(security);
  optimization_guide::proto::AgentContainerConfig* config =
      security->mutable_agent_container_config();
  EXPECT_TRUE(config);
  optimization_guide::proto::LocationRule* rule = config->add_location_rules();
  optimization_guide::proto::Site* site =
      rule->mutable_location()->mutable_site();
  EXPECT_TRUE(site);
  site->set_protocol(optimization_guide::proto::Protocol::PROTOCOL_HTTPS);
  site->set_domain("example.com");
  config->mutable_location_rules(0)->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  config->mutable_location_rules(0)
      ->mutable_metadata()
      ->add_accessible_resources(
          optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);

  ActorTaskMetadata metadata(actions);
  EXPECT_TRUE(metadata.agent_container_config().has_value());
}

}  // namespace actor
