// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task_metadata.h"

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

}  // namespace actor
