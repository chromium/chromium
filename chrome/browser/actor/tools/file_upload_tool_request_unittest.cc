// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_upload_tool_request.h"

#include <memory>
#include <sstream>
#include <vector>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/actor/tool_request_variant.h"
#include "chrome/browser/actor/tools/file_upload_tool.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

TEST(FileUploadToolRequestTest, Properties) {
  PageTarget target = DomNode{.node_id = 123, .document_identifier = "doc"};
  std::vector<FileUploadSource> files = {
      FileUploadSource(GURL("https://example.com/file.txt")),
  };

  FileUploadToolRequest request(ActorSurfaceHandle::CreateForTesting(), target,
                                files);

  EXPECT_EQ(request.Name(), FileUploadToolRequest::kName);
  EXPECT_EQ(request.Name(), "FileUpload");
  EXPECT_EQ(request.JournalEvent(), "FileUpload");
  EXPECT_EQ(request.files().size(), 1u);
  EXPECT_EQ(request.files()[0].url, GURL("https://example.com/file.txt"));
}

TEST(FileUploadToolRequestTest, OperatorStreamRedactsUrl) {
  FileUploadSource source(GURL("https://example.com/secret/path?token=secret"));

  std::ostringstream oss;
  oss << source;
  EXPECT_EQ(oss.str(), "Url(https://example.com/<redacted>)");

  std::ostringstream vec_oss;
  vec_oss << std::vector<FileUploadSource>{source};
  EXPECT_EQ(vec_oss.str(), "[Url(https://example.com/<redacted>)]");
}

TEST(FileUploadToolRequestTest, CreateToolNullTabReturnsError) {
  MockToolDelegate delegate;
  // No surface is registered under this handle.
  FileUploadToolRequest request(
      ActorSurfaceHandle::CreateForTesting(),
      DomNode{.node_id = 1, .document_identifier = "doc"}, {});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code, mojom::ActionResultCode::kTabWentAway);
}

TEST(FileUploadToolRequestTest, CreateTool_EmptyFileListFails) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
  ActorSurfaceImpl actor_surface(ActorSurfaceHandle::CreateForTesting(),
                                 mock_tab.GetHandle());
  FileUploadToolRequest request(
      actor_surface.GetHandle(),
      DomNode{.node_id = 1, .document_identifier = "doc"},
      /*files=*/{});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code,
            mojom::ActionResultCode::kFileUploadEmptyFileList);
}

TEST(FileUploadToolRequestTest, CreateTool_ValidUrlSucceeds) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
  ActorSurfaceImpl actor_surface(ActorSurfaceHandle::CreateForTesting(),
                                 mock_tab.GetHandle());
  FileUploadToolRequest request(
      actor_surface.GetHandle(),
      DomNode{.node_id = 1, .document_identifier = "doc"},
      {FileUploadSource(GURL("https://example.com/file.pdf"))});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_NE(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code, mojom::ActionResultCode::kOk);
}

TEST(FileUploadToolRequestTest, CreateTool_NonHttpUrlFails) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
  ActorSurfaceImpl actor_surface(ActorSurfaceHandle::CreateForTesting(),
                                 mock_tab.GetHandle());
  FileUploadToolRequest request(
      actor_surface.GetHandle(),
      DomNode{.node_id = 1, .document_identifier = "doc"},
      {FileUploadSource(GURL("file:///etc/passwd"))});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code,
            mojom::ActionResultCode::kFileUploadUnauthorizedFile);
}

TEST(FileUploadToolRequestTest, CreateTool_InvalidUrlFails) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
  ActorSurfaceImpl actor_surface(ActorSurfaceHandle::CreateForTesting(),
                                 mock_tab.GetHandle());
  FileUploadToolRequest request(
      actor_surface.GetHandle(),
      DomNode{.node_id = 1, .document_identifier = "doc"},
      {FileUploadSource(GURL("not a url"))});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code,
            mojom::ActionResultCode::kFileUploadUnauthorizedFile);
}

TEST(FileUploadToolRequestTest, VariantConversion) {
  PageTarget target = DomNode{.node_id = 123, .document_identifier = "doc"};
  std::vector<FileUploadSource> files;
  FileUploadToolRequest request(ActorSurfaceHandle::CreateForTesting(), target,
                                files);

  EXPECT_TRUE(
      std::holds_alternative<FileUploadToolRequest>(ConvertToVariant(request)));
}

}  // namespace
}  // namespace actor
