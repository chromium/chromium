// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/android/family_info_feedback_source.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/test/test_support_jni_headers/FamilyInfoFeedbackSourceTestBridge_jni.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace chrome::android {
namespace {

const char kTestEmail[] = "test@gmail.com";

}  // namespace

class FamilyInfoFeedbackSourceTest
    : public testing::TestWithParam<FamilyInfoFetcher::FamilyMemberRole> {
 public:
  FamilyInfoFeedbackSourceTest() : env_(base::android::AttachCurrentThread()) {}

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  // Methods to access Java counterpart FamilyInfoFeedbackSource.
  std::string GetFeedbackForTesting(
      ScopedJavaLocalRef<jobject> j_feedback_source) {
    const base::android::JavaRef<jstring>& j_value =
        Java_FamilyInfoFeedbackSourceTestBridge_getValue(
            env_,
            base::android::JavaParamRef<jobject>(env_, j_feedback_source.obj()),
            base::android::ConvertUTF8ToJavaString(env_, "Family_Member_Role"));
    return base::android::ConvertJavaStringToUTF8(env_, j_value);
  }

  void GetFamilyMembersSuccess(
      ScopedJavaLocalRef<jobject> j_feedback_source,
      const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
    FamilyInfoFeedbackSource* feedback_source = new FamilyInfoFeedbackSource(
        base::android::JavaParamRef<jobject>(env_, j_feedback_source.obj()),
        profile_.get());
    feedback_source->OnGetFamilyMembersSuccess(members);
  }

  void GetFamilyMembersFailure(ScopedJavaLocalRef<jobject> j_feedback_source) {
    FamilyInfoFeedbackSource* feedback_source = new FamilyInfoFeedbackSource(
        base::android::JavaParamRef<jobject>(env_, j_feedback_source.obj()),
        profile_.get());
    feedback_source->OnFailure(FamilyInfoFetcher::ErrorCode::kTokenError);
  }

  base::android::ScopedJavaLocalRef<jobject> CreateJavaObjectForTesting() {
    ProfileAndroid* profile_android =
        ProfileAndroid::FromProfile(profile_.get());
    return Java_FamilyInfoFeedbackSourceTestBridge_createFamilyInfoFeedbackSource(
        env_, base::android::JavaParamRef<jobject>(
                  env_, profile_android->GetJavaObject().Release()));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  raw_ptr<JNIEnv> env_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
};

// Tests that the family role for a user in a Family Group is recorded.
TEST_P(FamilyInfoFeedbackSourceTest, GetFamilyMembersSignedIn) {
  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);
  base::android::ScopedJavaLocalRef<jobject> j_feedback_source =
      CreateJavaObjectForTesting();

  FamilyInfoFetcher::FamilyMemberRole role = GetParam();
  std::vector<FamilyInfoFetcher::FamilyMember> members(
      {FamilyInfoFetcher::FamilyMember(
          primary_account.gaia, role, "Name", kTestEmail,
          /*profile_url=*/std::string(), /*profile_image_url=*/std::string())});

  GetFamilyMembersSuccess(j_feedback_source, members);

  std::string expected_role =
      role == FamilyInfoFetcher::FamilyMemberRole::HEAD_OF_HOUSEHOLD
          ? "family_manager"
          : FamilyInfoFetcher::RoleToString(role);
  EXPECT_EQ(expected_role, GetFeedbackForTesting(j_feedback_source));
}

// Tests that a user that is not in a Family group is not processed.
TEST_F(FamilyInfoFeedbackSourceTest, GetFamilyMembersSignedInNoFamily) {
  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);
  base::android::ScopedJavaLocalRef<jobject> j_feedback_source =
      CreateJavaObjectForTesting();

  std::vector<FamilyInfoFetcher::FamilyMember> members;
  GetFamilyMembersSuccess(j_feedback_source, members);

  EXPECT_EQ("", GetFeedbackForTesting(j_feedback_source));
}

// Tests that a signed-in user that fails its request to the server is not
// processed.
TEST_F(FamilyInfoFeedbackSourceTest, GetFamilyMembersOnFailure) {
  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);
  base::android::ScopedJavaLocalRef<jobject> j_feedback_source =
      CreateJavaObjectForTesting();

  GetFamilyMembersFailure(j_feedback_source);

  EXPECT_EQ("", GetFeedbackForTesting(j_feedback_source));
}

INSTANTIATE_TEST_SUITE_P(
    AllFamilyMemberRoles,
    FamilyInfoFeedbackSourceTest,
    ::testing::Values(FamilyInfoFetcher::FamilyMemberRole::HEAD_OF_HOUSEHOLD,
                      FamilyInfoFetcher::FamilyMemberRole::CHILD,
                      FamilyInfoFetcher::FamilyMemberRole::MEMBER,
                      FamilyInfoFetcher::FamilyMemberRole::PARENT));

}  // namespace chrome::android
