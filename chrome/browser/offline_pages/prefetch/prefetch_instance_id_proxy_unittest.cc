// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_instance_id_proxy.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "components/gcm_driver/instance_id/instance_id_android.h"
#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"
#endif  // OS_ANDROID

using instance_id::InstanceID;

namespace offline_pages {
namespace {
const char kAppIdForTest[] = "com.google.test.PrefetchInstanceIDProxyTest";
}

class PrefetchInstanceIDProxyTest : public testing::Test {
 public:
  PrefetchInstanceIDProxyTest() = default;
  ~PrefetchInstanceIDProxyTest() override = default;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void WaitForAsyncOperation();

  // Sync wrapper for async version.
  std::string GetToken();

  PrefetchInstanceIDProxy* proxy() { return proxy_.get(); }

 private:
  void GetTokenCompleted(const std::string& token, InstanceID::Result result);

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<PrefetchInstanceIDProxy> proxy_;

  gcm::FakeGCMProfileService* gcm_profile_service_;

#if defined(OS_ANDROID)
  instance_id::InstanceIDAndroid::ScopedBlockOnAsyncTasksForTesting
      block_async_;
  instance_id::ScopedUseFakeInstanceIDAndroid use_fake_;
#endif  // OS_ANDROID

  std::string token_;
  InstanceID::Result result_ = InstanceID::UNKNOWN_ERROR;

  bool async_operation_completed_ = false;
  base::Closure async_operation_completed_callback_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchInstanceIDProxyTest);
};

void PrefetchInstanceIDProxyTest::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);
  proxy_ = std::make_unique<PrefetchInstanceIDProxy>(kAppIdForTest, &profile_);
  gcm_profile_service_ = static_cast<gcm::FakeGCMProfileService*>(
      gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile_, base::BindRepeating(&gcm::FakeGCMProfileService::Build)));
}

void PrefetchInstanceIDProxyTest::TearDown() {
  base::RunLoop().RunUntilIdle();
}

void PrefetchInstanceIDProxyTest::WaitForAsyncOperation() {
  // No need to wait if async operation is not needed.
  if (async_operation_completed_)
    return;
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

std::string PrefetchInstanceIDProxyTest::GetToken() {
  async_operation_completed_ = false;
  token_.clear();
  result_ = InstanceID::UNKNOWN_ERROR;

  proxy()->GetGCMToken(base::Bind(
      &PrefetchInstanceIDProxyTest::GetTokenCompleted, base::Unretained(this)));
  WaitForAsyncOperation();
  return token_;
}

void PrefetchInstanceIDProxyTest::GetTokenCompleted(const std::string& token,
                                                    InstanceID::Result result) {
  DCHECK(!async_operation_completed_);
  async_operation_completed_ = true;
  token_ = token;
  result_ = result;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

TEST_F(PrefetchInstanceIDProxyTest, GetToken) {
  std::string token = GetToken();
  EXPECT_NE("", token);
}

}  // namespace offline_pages
