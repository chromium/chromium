// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/android/family_info_feedback_source.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/supervised_user/child_accounts/family_info_fetcher.h"
#include "chrome/browser/supervised_user/supervised_user_denylist.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/test/test_support_jni_headers/FamilyInfoFeedbackSourceTestBridge_jni.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace chrome::android {
namespace {

const char kTestEmail[] = "test@gmail.com";
const char kFeedbackTagFamilyMemberRole[] = "Family_Member_Role";
const char kFeedbackTagParentalControlSitesChild[] =
    "Parental_Control_Sites_Child";
}  // namespace

class FamilyInfoFeedbackSourceForChildFilterBehaviorTest
    : public testing::TestWithParam<
          std::tuple<SupervisedUserURLFilter::FilteringBehavior, bool>> {
 public:
  FamilyInfoFeedbackSourceForChildFilterBehaviorTest()
      : env_(base::android::AttachCurrentThread()) {}

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    builder.SetIsSupervisedProfile();

    role_ = FamilyInfoFetcher::FamilyMemberRole::CHILD;
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    j_feedback_source_ = CreateJavaObjectForTesting();
    supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  // Methods to access Java counterpart FamilyInfoFeedbackSource.
  std::string GetFeedbackValue(std::string feedback_tag) {
    const base::android::JavaRef<jstring>& j_value =
        Java_FamilyInfoFeedbackSourceTestBridge_getValue(
            env_,
            base::android::JavaParamRef<jobject>(env_,
                                                 j_feedback_source_.obj()),
            base::android::ConvertUTF8ToJavaString(env_, feedback_tag));
    return base::android::ConvertJavaStringToUTF8(env_, j_value);
  }

  void OnGetFamilyMembersSuccess(
      base::WeakPtr<FamilyInfoFeedbackSource> feedback_source,
      const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
    feedback_source->OnGetFamilyMembersSuccess(members);
  }

  // Creates a new instance of FamilyInfoFeedbackSource that is destroyed on
  // completion of OnGetFamilyMembers* methods.
  base::WeakPtr<FamilyInfoFeedbackSource> CreateFamilyInfoFeedbackSource() {
    FamilyInfoFeedbackSource* source = new FamilyInfoFeedbackSource(
        base::android::JavaParamRef<jobject>(env_, j_feedback_source_.obj()),
        profile_.get());
    return source->weak_factory_.GetWeakPtr();
  }

  // Returns the expected string representation of the Parental_Control_
  // Sites_Parent PSD field that is included in the feedback log.
  std::string GetFilterTypeAsString(
      SupervisedUserURLFilter::FilteringBehavior behavior,
      bool safe_sites_enabled) {
    switch (behavior) {
      case (SupervisedUserURLFilter::FilteringBehavior::BLOCK):
        return "allow_certain_sites";
      case (SupervisedUserURLFilter::FilteringBehavior::ALLOW):
        if (safe_sites_enabled) {
          return "block_mature_sites";
        } else {
          return "allow_all_sites";
        }
      case (SupervisedUserURLFilter::FilteringBehavior::INVALID):
        return "";
    }
  }

  FamilyInfoFetcher::FamilyMemberRole role_;
  raw_ptr<SupervisedUserService> supervised_user_service_;
  SupervisedUserDenylist deny_list_ = SupervisedUserDenylist();

 private:
  // Creates a Java instance of FamilyInfoFeedbackSource.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObjectForTesting() {
    ProfileAndroid* profile_android =
        ProfileAndroid::FromProfile(profile_.get());
    return Java_FamilyInfoFeedbackSourceTestBridge_createFamilyInfoFeedbackSource(
        env_, base::android::JavaParamRef<jobject>(
                  env_, profile_android->GetJavaObject().Release()));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::android::ScopedJavaLocalRef<jobject> j_feedback_source_;
  raw_ptr<JNIEnv> env_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<TestingProfile> profile_;
};

// Tests that the parental control sites value for a child user is recorded.
TEST_P(FamilyInfoFeedbackSourceForChildFilterBehaviorTest,
       GetChildFilteringBehaviour) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      chrome::android::kReportParentalControlSitesChild);

  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

  SupervisedUserURLFilter::FilteringBehavior filtering_behavior =
      std::get<0>(GetParam());
  supervised_user_service_->GetURLFilter()->SetDefaultFilteringBehavior(
      filtering_behavior);

  bool safe_sites_enabled = std::get<1>(GetParam());
  if (safe_sites_enabled) {
    supervised_user_service_->GetURLFilter()->SetDenylist(&deny_list_);
  }

  std::vector<FamilyInfoFetcher::FamilyMember> members(
      {FamilyInfoFetcher::FamilyMember(
          primary_account.gaia, role_, "Name", kTestEmail,
          /*profile_url=*/std::string(), /*profile_image_url=*/std::string())});

  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnGetFamilyMembersSuccess(feedback_source, members);

  std::string expected_filter =
      GetFilterTypeAsString(filtering_behavior, safe_sites_enabled);
  EXPECT_EQ(expected_filter,
            GetFeedbackValue(kFeedbackTagParentalControlSitesChild));
}

INSTANTIATE_TEST_SUITE_P(
    FilterBehaviourContainer,
    FamilyInfoFeedbackSourceForChildFilterBehaviorTest,
    ::testing::Values(
        std::make_tuple(SupervisedUserURLFilter::FilteringBehavior::BLOCK,
                        false),
        std::make_tuple(SupervisedUserURLFilter::FilteringBehavior::BLOCK,
                        true),
        std::make_tuple(SupervisedUserURLFilter::FilteringBehavior::ALLOW,
                        false),
        std::make_tuple(SupervisedUserURLFilter::FilteringBehavior::ALLOW,
                        true)));

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
    j_feedback_source_ = CreateJavaObjectForTesting();
  }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  // Methods to access Java counterpart FamilyInfoFeedbackSource.
  std::string GetFeedbackValue() {
    const base::android::JavaRef<jstring>& j_value =
        Java_FamilyInfoFeedbackSourceTestBridge_getValue(
            env_,
            base::android::JavaParamRef<jobject>(env_,
                                                 j_feedback_source_.obj()),
            base::android::ConvertUTF8ToJavaString(
                env_, kFeedbackTagFamilyMemberRole));
    return base::android::ConvertJavaStringToUTF8(env_, j_value);
  }

  void OnGetFamilyMembersSuccess(
      base::WeakPtr<FamilyInfoFeedbackSource> feedback_source,
      const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
    feedback_source->OnGetFamilyMembersSuccess(members);
  }

  void OnGetFamilyMembersFailure(
      base::WeakPtr<FamilyInfoFeedbackSource> feedback_source) {
    feedback_source->OnFailure(FamilyInfoFetcher::ErrorCode::kTokenError);
  }

  // Creates a new instance of FamilyInfoFeedbackSource that is destroyed on
  // completion of OnGetFamilyMembers* methods.
  base::WeakPtr<FamilyInfoFeedbackSource> CreateFamilyInfoFeedbackSource() {
    FamilyInfoFeedbackSource* source = new FamilyInfoFeedbackSource(
        base::android::JavaParamRef<jobject>(env_, j_feedback_source_.obj()),
        profile_.get());
    return source->weak_factory_.GetWeakPtr();
  }

 private:
  // Creates a Java instance of FamilyInfoFeedbackSource.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObjectForTesting() {
    ProfileAndroid* profile_android =
        ProfileAndroid::FromProfile(profile_.get());
    return Java_FamilyInfoFeedbackSourceTestBridge_createFamilyInfoFeedbackSource(
        env_, base::android::JavaParamRef<jobject>(
                  env_, profile_android->GetJavaObject().Release()));
  }

  content::BrowserTaskEnvironment task_environment_;

  base::android::ScopedJavaLocalRef<jobject> j_feedback_source_;
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

  FamilyInfoFetcher::FamilyMemberRole role = GetParam();
  std::vector<FamilyInfoFetcher::FamilyMember> members(
      {FamilyInfoFetcher::FamilyMember(
          primary_account.gaia, role, "Name", kTestEmail,
          /*profile_url=*/std::string(), /*profile_image_url=*/std::string())});

  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnGetFamilyMembersSuccess(feedback_source, members);

  std::string expected_role =
      role == FamilyInfoFetcher::FamilyMemberRole::HEAD_OF_HOUSEHOLD
          ? "family_manager"
          : FamilyInfoFetcher::RoleToString(role);
  EXPECT_EQ(expected_role, GetFeedbackValue());
}

// Tests that a user that is not in a Family group is not processed.
TEST_F(FamilyInfoFeedbackSourceTest, GetFamilyMembersSignedInNoFamily) {
  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  std::vector<FamilyInfoFetcher::FamilyMember> members;
  OnGetFamilyMembersSuccess(feedback_source, members);

  EXPECT_EQ("", GetFeedbackValue());
}

// Tests that a signed-in user that fails its request to the server is not
// processed.
TEST_F(FamilyInfoFeedbackSourceTest, GetFamilyMembersOnFailure) {
  CoreAccountInfo primary_account =
      identity_test_env()->MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnGetFamilyMembersFailure(feedback_source);

  EXPECT_EQ("", GetFeedbackValue());
}

TEST_F(FamilyInfoFeedbackSourceTest, FeedbackSourceDestroyedOnCompletion) {
  std::vector<FamilyInfoFetcher::FamilyMember> members;
  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnGetFamilyMembersSuccess(feedback_source, members);

  EXPECT_TRUE(feedback_source.WasInvalidated());
}

TEST_F(FamilyInfoFeedbackSourceTest, FeedbackSourceDestroyedOnFailure) {
  base::WeakPtr<FamilyInfoFeedbackSource> feedback_source =
      CreateFamilyInfoFeedbackSource();
  OnGetFamilyMembersFailure(feedback_source);

  EXPECT_TRUE(feedback_source.WasInvalidated());
}

INSTANTIATE_TEST_SUITE_P(
    AllFamilyMemberRoles,
    FamilyInfoFeedbackSourceTest,
    ::testing::Values(FamilyInfoFetcher::FamilyMemberRole::HEAD_OF_HOUSEHOLD,
                      FamilyInfoFetcher::FamilyMemberRole::CHILD,
                      FamilyInfoFetcher::FamilyMemberRole::MEMBER,
                      FamilyInfoFetcher::FamilyMemberRole::PARENT));

}  // namespace chrome::android
