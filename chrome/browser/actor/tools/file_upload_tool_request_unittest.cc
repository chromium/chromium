// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_upload_tool_request.h"

#include <memory>
#include <sstream>
#include <vector>

#include "base/files/file_path.h"
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
  tabs::MockTabInterface mock_tab;
  PageTarget target = DomNode{.node_id = 123, .document_identifier = "doc"};
  std::vector<FileUploadSource> files = {
      FileUploadSource{
          .type = FileUploadSource::Type::kLocalPath,
          .local_path = base::FilePath(FILE_PATH_LITERAL("/path/to/file.txt"))},
  };

  FileUploadToolRequest request(mock_tab.GetHandle(), target, files);

  EXPECT_EQ(request.Name(), FileUploadToolRequest::kName);
  EXPECT_EQ(request.Name(), "FileUpload");
  EXPECT_EQ(request.JournalEvent(), "FileUpload");
  EXPECT_EQ(request.files().size(), 1u);
  EXPECT_EQ(request.files()[0].local_path,
            base::FilePath(FILE_PATH_LITERAL("/path/to/file.txt")));
}

TEST(FileUploadToolRequestTest, OperatorStreamRedactsPath) {
  FileUploadSource source{
      .type = FileUploadSource::Type::kLocalPath,
      .local_path =
          base::FilePath(FILE_PATH_LITERAL("/home/secret/user_data.pdf")),
  };

  std::ostringstream oss;
  oss << source;
  EXPECT_EQ(oss.str(), "LocalPath(<redacted>)");

  std::ostringstream vec_oss;
  vec_oss << std::vector<FileUploadSource>{source};
  EXPECT_EQ(vec_oss.str(), "[LocalPath(<redacted>)]");
}

TEST(FileUploadToolRequestTest, CreateToolNullTabReturnsError) {
  MockToolDelegate delegate;
  tabs::TabHandle invalid_tab_handle(999);
  FileUploadToolRequest request(
      invalid_tab_handle, DomNode{.node_id = 1, .document_identifier = "doc"},
      {});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code, mojom::ActionResultCode::kTabWentAway);
}

TEST(FileUploadToolRequestTest, CreateTool_EmptyFileListFails) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
  FileUploadToolRequest request(
      mock_tab.GetHandle(), DomNode{.node_id = 1, .document_identifier = "doc"},
      /*files=*/{});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code,
            mojom::ActionResultCode::kFileUploadEmptyFileList);
}

TEST(FileUploadToolRequestTest, CreateTool_EmptyLocalPathFails) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
  FileUploadSource source{
      .type = FileUploadSource::Type::kLocalPath,
      .local_path = base::FilePath(),
  };
  FileUploadToolRequest request(
      mock_tab.GetHandle(), DomNode{.node_id = 1, .document_identifier = "doc"},
      {source});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code,
            mojom::ActionResultCode::kFileUploadUnauthorizedFile);
}

TEST(FileUploadToolRequestTest, CreateTool_RelativeLocalPathFails) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
  FileUploadSource source{
      .type = FileUploadSource::Type::kLocalPath,
      .local_path = base::FilePath(FILE_PATH_LITERAL("relative/file.txt")),
  };
  FileUploadToolRequest request(
      mock_tab.GetHandle(), DomNode{.node_id = 1, .document_identifier = "doc"},
      {source});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code,
            mojom::ActionResultCode::kFileUploadUnauthorizedFile);
}

TEST(FileUploadToolRequestTest, CreateTool_ParentReferencingLocalPathFails) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
  FileUploadSource source{
      .type = FileUploadSource::Type::kLocalPath,
      .local_path =
          base::FilePath(FILE_PATH_LITERAL("/var/log/../../etc/passwd")),
  };
  FileUploadToolRequest request(
      mock_tab.GetHandle(), DomNode{.node_id = 1, .document_identifier = "doc"},
      {source});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_EQ(result.tool, nullptr);
  ASSERT_TRUE(result.result);
  EXPECT_EQ(result.result->code,
            mojom::ActionResultCode::kFileUploadUnauthorizedFile);
}

TEST(FileUploadToolRequestTest, CreateTool_ValidPathSucceeds) {
  MockToolDelegate delegate;
  tabs::MockTabInterface mock_tab;
#if BUILDFLAG(IS_WIN)
  base::FilePath valid_path(FILE_PATH_LITERAL("C:\\Users\\test\\file.pdf"));
#else
  base::FilePath valid_path(FILE_PATH_LITERAL("/home/test/file.pdf"));
#endif
  FileUploadSource source{
      .type = FileUploadSource::Type::kLocalPath,
      .local_path = valid_path,
  };
  FileUploadToolRequest request(
      mock_tab.GetHandle(), DomNode{.node_id = 1, .document_identifier = "doc"},
      {source});

  auto result = request.CreateTool(TaskId(1), delegate);
  EXPECT_NE(result.tool, nullptr);
  EXPECT_TRUE(result.result.is_null());
}

TEST(FileUploadToolRequestTest, VariantConversion) {
  tabs::MockTabInterface mock_tab;
  PageTarget target = DomNode{.node_id = 123, .document_identifier = "doc"};
  std::vector<FileUploadSource> files;
  FileUploadToolRequest request(mock_tab.GetHandle(), target, files);

  ConvertToVariantFn fn;
  request.Apply(fn);
  ASSERT_TRUE(fn.GetVariant().has_value());
  EXPECT_TRUE(std::holds_alternative<FileUploadToolRequest>(*fn.GetVariant()));
}

}  // namespace
}  // namespace actor
