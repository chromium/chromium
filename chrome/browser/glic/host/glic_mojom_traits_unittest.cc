// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_mojom_traits.h"

#include <optional>
#include <vector>

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_drive_picker_types.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicMojomTraitsTest, DriveFileSelectionRoundTrip) {
  GlicDriveFileSelection input;
  input.id = "file-1";
  input.title = "Q3 Report.pdf";
  input.mime_type = "application/pdf";
  input.size_bytes = 4096;
  input.resource_key = "rk-1";

  GlicDriveFileSelection output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DriveFileSelection>(
      input, output));
  EXPECT_EQ(output, input);
}

TEST(GlicMojomTraitsTest, DriveFileSelectionWithoutResourceKey) {
  GlicDriveFileSelection input;
  input.id = "doc-2";
  input.title = "Notes";
  input.mime_type = "application/vnd.google-apps.document";
  input.size_bytes = 0;

  GlicDriveFileSelection output;
  output.resource_key = "stale";
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DriveFileSelection>(
      input, output));
  EXPECT_EQ(output, input);
  EXPECT_EQ(output.resource_key, std::nullopt);
}

TEST(GlicMojomTraitsTest, DrivePickerResultErrorReasonRoundTrip) {
  for (GlicDrivePickerError error : {
           GlicDrivePickerError::kUnknown,
           GlicDrivePickerError::kWebSessionSignedOut,
           GlicDrivePickerError::kChromeSignInRequired,
           GlicDrivePickerError::kAccountMismatch,
           GlicDrivePickerError::kAlreadyActive,
           GlicDrivePickerError::kNoAnchorWindow,
           GlicDrivePickerError::kLoadFailed,
           GlicDrivePickerError::kDisabledByPolicy,
           GlicDrivePickerError::kUnsupported,
       }) {
    auto input = mojom::DrivePickerResult::NewErrorReason(error);
    mojom::DrivePickerResultPtr output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DrivePickerResult>(
        input, output));
    ASSERT_TRUE(output);
    ASSERT_TRUE(output->is_error_reason());
    EXPECT_EQ(output->get_error_reason(), error);
  }
}

TEST(GlicMojomTraitsTest, DrivePickerResultFilesRoundTrip) {
  GlicDriveFileSelection file;
  file.id = "file-1";
  file.title = "Q3 Report.pdf";
  file.mime_type = "application/pdf";
  file.size_bytes = 4096;
  file.resource_key = "rk-1";

  auto input = mojom::DrivePickerResult::NewFiles({file});
  mojom::DrivePickerResultPtr output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DrivePickerResult>(
      input, output));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_files());
  EXPECT_EQ(output->get_files(), std::vector<GlicDriveFileSelection>({file}));
}

TEST(GlicMojomTraitsTest, DrivePickerResultEmptyFilesRoundTrip) {
  auto input =
      mojom::DrivePickerResult::NewFiles(std::vector<GlicDriveFileSelection>());
  mojom::DrivePickerResultPtr output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DrivePickerResult>(
      input, output));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_files());
  EXPECT_TRUE(output->get_files().empty());
}

}  // namespace
}  // namespace glic
