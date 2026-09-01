// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_upload_tool.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

base::FilePath GetValidFilePath() {
#if BUILDFLAG(IS_WIN)
  return base::FilePath(FILE_PATH_LITERAL("C:\\Users\\test\\file.pdf"));
#else
  return base::FilePath(FILE_PATH_LITERAL("/home/test/file.pdf"));
#endif
}

class FileUploadToolTest : public testing::Test {
 public:
  FileUploadToolTest() = default;
  ~FileUploadToolTest() override = default;

  MockToolDelegate& delegate() { return delegate_; }
  tabs::MockTabInterface& mock_tab() { return mock_tab_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockToolDelegate delegate_;
  tabs::MockTabInterface mock_tab_;
};

TEST_F(FileUploadToolTest, Validate_Succeeds) {
  FileUploadSource source{
      .type = FileUploadSource::Type::kLocalPath,
      .local_path = GetValidFilePath(),
  };
  FileUploadTool tool(TaskId(1), delegate(), mock_tab(),
                      DomNode{.node_id = 1, .document_identifier = "doc"},
                      {source});

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Validate(future.GetCallback());
  ExpectOkResult(future);
}

TEST_F(FileUploadToolTest, TimeOfUseValidation_Succeeds) {
  FileUploadSource source{
      .type = FileUploadSource::Type::kLocalPath,
      .local_path = GetValidFilePath(),
  };
  FileUploadTool tool(TaskId(1), delegate(), mock_tab(),
                      DomNode{.node_id = 1, .document_identifier = "doc"},
                      {source});

  auto result = tool.TimeOfUseValidation(nullptr);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);
}

TEST_F(FileUploadToolTest, Invoke_Succeeds) {
  FileUploadSource source{
      .type = FileUploadSource::Type::kLocalPath,
      .local_path = GetValidFilePath(),
  };
  FileUploadTool tool(TaskId(1), delegate(), mock_tab(),
                      DomNode{.node_id = 1, .document_identifier = "doc"},
                      {source});

  base::test::TestFuture<mojom::ActionResultPtr> future;
  tool.Invoke(future.GetCallback());
  ExpectOkResult(future);
}

TEST_F(FileUploadToolTest, DebugString_NoPiiLeakage) {
  FileUploadSource source{
      .type = FileUploadSource::Type::kLocalPath,
      .local_path = GetValidFilePath(),
  };
  FileUploadTool tool(TaskId(1), delegate(), mock_tab(),
                      DomNode{.node_id = 1, .document_identifier = "doc"},
                      {source});

  EXPECT_EQ(tool.DebugString(), "FileUploadTool(files_count=1)");
}

}  // namespace
}  // namespace actor
