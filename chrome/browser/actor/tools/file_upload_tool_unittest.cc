// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_upload_tool.h"

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace actor {
namespace {

FileUploadSource UrlSource() {
  return FileUploadSource(GURL("https://example.com/file.pdf"));
}

class FileUploadToolTest : public testing::Test {
 public:
  FileUploadToolTest() {
    scoped_feature_list_.InitAndEnableFeature(kGlicActorFileUploadTool);
  }
  ~FileUploadToolTest() override = default;

  MockToolDelegate& delegate() { return delegate_; }
  ActorSurface& actor_surface() { return actor_surface_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockToolDelegate delegate_;
  tabs::MockTabInterface mock_tab_;
  ActorSurfaceImpl actor_surface_{ActorSurfaceHandle::CreateForTesting(),
                                  mock_tab_.GetHandle()};
};

TEST_F(FileUploadToolTest, Validate_Succeeds) {
  EXPECT_CALL(delegate(), IsAcceptableNavigationDestination)
      .WillOnce([](const GURL& url, DecisionCallbackWithReason callback) {
        std::move(callback).Run(MayActOnUrlBlockReason::kAllowed);
      });

  FileUploadTool tool(TaskId(1), delegate(), actor_surface(),
                      DomNode{.node_id = 1, .document_identifier = "doc"},
                      {UrlSource()});

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  ExpectOkResult(future);
}

TEST_F(FileUploadToolTest, Validate_FailsForBlockedUrl) {
  EXPECT_CALL(delegate(), IsAcceptableNavigationDestination)
      .WillOnce([](const GURL& url, DecisionCallbackWithReason callback) {
        std::move(callback).Run(MayActOnUrlBlockReason::kSafeBrowsing);
      });

  FileUploadTool tool(TaskId(1), delegate(), actor_surface(),
                      DomNode{.node_id = 1, .document_identifier = "doc"},
                      {UrlSource()});

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  EXPECT_NE(future.Get()->code, mojom::ActionResultCode::kOk);
}

TEST_F(FileUploadToolTest, Validate_FailsWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(kGlicActorFileUploadTool);

  FileUploadTool tool(TaskId(1), delegate(), actor_surface(),
                      DomNode{.node_id = 1, .document_identifier = "doc"},
                      {UrlSource()});

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  EXPECT_EQ(future.Get()->code, mojom::ActionResultCode::kToolUnknown);
}

TEST_F(FileUploadToolTest, Validate_FailsWithNoFiles) {
  FileUploadTool tool(TaskId(1), delegate(), actor_surface(),
                      DomNode{.node_id = 1, .document_identifier = "doc"}, {});

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  EXPECT_EQ(future.Get()->code,
            mojom::ActionResultCode::kFileUploadEmptyFileList);
}

TEST_F(FileUploadToolTest, TimeOfUseValidation_Succeeds) {
  FileUploadTool tool(TaskId(1), delegate(), actor_surface(),
                      DomNode{.node_id = 1, .document_identifier = "doc"},
                      {UrlSource()});

  auto result = tool.TimeOfUseValidation(nullptr);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);
}

// Without a live WebContents there is nothing to stage the files on. End to end
// coverage of a successful invocation lives in file_upload_tool_browsertest.cc.
TEST_F(FileUploadToolTest, Invoke_FailsWithoutWebContents) {
  FileUploadTool tool(TaskId(1), delegate(), actor_surface(),
                      DomNode{.node_id = 1, .document_identifier = "doc"},
                      {UrlSource()});

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Invoke(future.GetCallback());
  EXPECT_EQ(future.Get()->code, mojom::ActionResultCode::kTabWentAway);
}

TEST_F(FileUploadToolTest, DebugString_NoPiiLeakage) {
  FileUploadTool tool(
      TaskId(1), delegate(), actor_surface(),
      DomNode{.node_id = 1, .document_identifier = "doc"},
      {FileUploadSource(GURL("https://example.com/secret/file.pdf"))});

  EXPECT_EQ(tool.DebugString(), "FileUploadTool(files_count=1)");
}

}  // namespace
}  // namespace actor
