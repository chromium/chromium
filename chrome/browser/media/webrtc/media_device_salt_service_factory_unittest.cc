// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"

#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_device_salt/media_device_salt_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

TEST(MediaDeviceSaltServiceFactoryTest, Test) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  MediaDeviceSaltServiceFactory* factory =
      MediaDeviceSaltServiceFactory::GetInstance();
  ASSERT_TRUE(factory);

  auto* service = factory->GetForBrowserContext(&profile);
  ASSERT_TRUE(service);

  base::test::TestFuture<const std::string&> future;
  service->GetSalt(
      blink::StorageKey::CreateFromStringForTesting("https://example.com"),
      future.GetCallback());
  EXPECT_FALSE(future.Get().empty());
}
