// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/destroy_partitions_operation.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"

namespace extensions {
namespace image_writer {
namespace {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;

class ImageWriterDestroyPartitionsOperationTest
    : public ImageWriterUnitTestBase {};

TEST_F(ImageWriterDestroyPartitionsOperationTest, EndToEnd) {
  TestingProfile profile;
  MockOperationManager manager(&profile);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  auto set_up_utility_client_progress_simulation =
      [](FakeImageWriterClient* client) {
        std::vector<int> progress_list{0, 50, 100};
        bool will_succeed = true;
        client->SimulateProgressOnWrite(progress_list, will_succeed);
        client->SimulateProgressOnVerifyWrite(progress_list, will_succeed);
      };
  // Sets up client for simulating Operation::Progress() on Operation::Write and
  // Operation::VerifyWrite.
  test_utils_.RunOnUtilityClientCreation(
      base::BindOnce(set_up_utility_client_progress_simulation));
#endif

  scoped_refptr<DestroyPartitionsOperation> operation(
      new DestroyPartitionsOperation(
          manager.AsWeakPtr(), kDummyExtensionId,
          test_utils_.GetDevicePath().AsUTF8Unsafe(),
          base::FilePath(FILE_PATH_LITERAL("/var/tmp"))));

  EXPECT_CALL(manager, OnProgress(kDummyExtensionId,
                                  image_writer_api::Stage::kVerifyWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, _))
      .Times(AnyNumber());
  EXPECT_CALL(manager,
              OnProgress(kDummyExtensionId, image_writer_api::Stage::kWrite, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager, OnProgress(kDummyExtensionId,
                                  image_writer_api::Stage::kWrite, 100))
      .Times(AtLeast(1));
  EXPECT_CALL(manager, OnComplete(kDummyExtensionId)).Times(1);
  EXPECT_CALL(manager, OnError(kDummyExtensionId, _, _, _)).Times(0);

  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  content::RunAllTasksUntilIdle();
}

} // namespace
} // namespace image_writer
} // namespace extensions
