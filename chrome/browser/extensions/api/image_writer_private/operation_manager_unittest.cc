// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/test_event_router.h"

namespace extensions {
namespace image_writer {

namespace {

class ImageWriterOperationManagerTest : public ImageWriterUnitTestBase {
 public:
  void StartCallback(bool success, const std::string& error) {
    started_ = true;
    start_success_ = success;
    start_error_ = error;
  }

  void CancelCallback(bool success, const std::string& error) {
    cancelled_ = true;
    cancel_success_ = true;
    cancel_error_ = error;
  }

 protected:
  ImageWriterOperationManagerTest()
      : started_(false),
        start_success_(false) {
  }

  void SetUp() override {
    ImageWriterUnitTestBase::SetUp();
    CreateAndUseTestEventRouter(&test_profile_);
  }

  bool started_;
  bool start_success_;
  std::string start_error_;

  bool cancelled_;
  bool cancel_success_;
  std::string cancel_error_;

  TestingProfile test_profile_;
};

TEST_F(ImageWriterOperationManagerTest, WriteFromFile) {
  OperationManager manager(&test_profile_);

  manager.StartWriteFromFile(
      kDummyExtensionId, test_utils_.GetImagePath(),
      test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::BindOnce(&ImageWriterOperationManagerTest::StartCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(started_);
  EXPECT_TRUE(start_success_);
  EXPECT_EQ("", start_error_);

  manager.CancelWrite(
      kDummyExtensionId,
      base::BindOnce(&ImageWriterOperationManagerTest::CancelCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(cancelled_);
  EXPECT_TRUE(cancel_success_);
  EXPECT_EQ("", cancel_error_);

  content::RunAllTasksUntilIdle();
}

TEST_F(ImageWriterOperationManagerTest, DestroyPartitions) {
  OperationManager manager(&test_profile_);

  manager.DestroyPartitions(
      kDummyExtensionId, test_utils_.GetDevicePath().AsUTF8Unsafe(),
      base::BindOnce(&ImageWriterOperationManagerTest::StartCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(started_);
  EXPECT_TRUE(start_success_);
  EXPECT_EQ("", start_error_);

  manager.CancelWrite(
      kDummyExtensionId,
      base::BindOnce(&ImageWriterOperationManagerTest::CancelCallback,
                     base::Unretained(this)));

  EXPECT_TRUE(cancelled_);
  EXPECT_TRUE(cancel_success_);
  EXPECT_EQ("", cancel_error_);

  content::RunAllTasksUntilIdle();
}

} // namespace
} // namespace image_writer
} // namespace extensions
