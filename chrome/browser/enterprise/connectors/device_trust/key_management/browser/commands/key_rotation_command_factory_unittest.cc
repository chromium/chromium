// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace enterprise_connectors {

class KeyRotationCommandFactoryTest : public testing::Test,
                                      public testing::WithParamInterface<bool> {
 protected:
  KeyRotationCommandFactoryTest() {
    feature_list_.InitWithFeatureState(
        enterprise_connectors::kDTCKeyRotationUploadedBySharedAPIEnabled,
        is_key_uploaded_by_shared_api());
  }

  base::test::ScopedFeatureList feature_list_;
  bool is_key_uploaded_by_shared_api() { return GetParam(); }
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  testing::StrictMock<policy::MockJobCreationHandler> job_creation_handler_;
  policy::FakeDeviceManagementService fake_device_management_service_{
      &job_creation_handler_};
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
};

TEST_P(KeyRotationCommandFactoryTest, CreateCommand) {
  // This test will run on different platforms.
  ASSERT_TRUE(KeyRotationCommandFactory::GetInstance()->CreateCommand(
      test_shared_loader_factory_, &fake_device_management_service_));
}

INSTANTIATE_TEST_SUITE_P(, KeyRotationCommandFactoryTest, testing::Bool());

}  // namespace enterprise_connectors
