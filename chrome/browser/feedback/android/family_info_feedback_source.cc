// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/android/family_info_feedback_source.h"

#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/feedback/android/jni_headers/FamilyInfoFeedbackSource_jni.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using FamilyMemberRole = FamilyInfoFetcher::FamilyMemberRole;

namespace chrome::android {
namespace {

// User visible role name for FamilyMember::HEAD_OF_HOUSEHOLD.
const char kFamilyManagerRole[] = "family_manager";

}  // namespace

void JNI_FamilyInfoFeedbackSource_Start(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FamilyInfoFeedbackSource* feedback_source =
      new FamilyInfoFeedbackSource(obj, profile);
  feedback_source->GetFamilyMembers();
}

FamilyInfoFeedbackSource::FamilyInfoFeedbackSource(
    const JavaParamRef<jobject>& obj,
    Profile* profile)
    : supervised_user_service_(
          SupervisedUserServiceFactory::GetForProfile(profile)),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      url_loader_factory_(profile->GetDefaultStoragePartition()
                              ->GetURLLoaderFactoryForBrowserProcess()),
      java_ref_(obj) {}

FamilyInfoFeedbackSource::~FamilyInfoFeedbackSource() = default;

void FamilyInfoFeedbackSource::GetFamilyMembers() {
  family_fetcher_ = std::make_unique<FamilyInfoFetcher>(this, identity_manager_,
                                                        url_loader_factory_);
  family_fetcher_->StartGetFamilyMembers();
}

void FamilyInfoFeedbackSource::OnGetFamilyMembersSuccess(
    const std::vector<FamilyInfoFetcher::FamilyMember>& members) {
  std::string primary_account_gaia =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  JNIEnv* env = AttachCurrentThread();
  for (const FamilyInfoFetcher::FamilyMember& member : members) {
    // Store the family member role for the primary account of the profile.
    if (primary_account_gaia == member.obfuscated_gaia_id) {
      std::string role = member.role == FamilyMemberRole::HEAD_OF_HOUSEHOLD
                             ? kFamilyManagerRole
                             : FamilyInfoFetcher::RoleToString(member.role);

      // If a child is signed-in, report the parental control web filter.
      ScopedJavaLocalRef<jstring> child_web_filter_type = nullptr;
      if (base::FeatureList::IsEnabled(kReportParentalControlSitesChild) &&
          member.role == FamilyMemberRole::CHILD) {
        SupervisedUserURLFilter::WebFilterType web_filter_type =
            supervised_user_service_->GetURLFilter()->GetWebFilterType();
        child_web_filter_type = ConvertUTF8ToJavaString(
            env, SupervisedUserURLFilter::WebFilterTypeToDisplayString(
                     web_filter_type));
      }
      Java_FamilyInfoFeedbackSource_processPrimaryAccountFamilyInfo(
          env, java_ref_, ConvertUTF8ToJavaString(env, role),
          child_web_filter_type);
    }
  }
  OnGetFamilyMembersCompletion();
}

void FamilyInfoFeedbackSource::OnFailure(FamilyInfoFetcher::ErrorCode error) {
  DLOG(WARNING) << "GetFamilyMembers failed with code "
                << static_cast<int>(error);
  OnGetFamilyMembersCompletion();
}

void FamilyInfoFeedbackSource::OnGetFamilyMembersCompletion() {
  // Object will delete itself following the fetch to GetFamilyMembers.
  delete this;
}

}  // namespace chrome::android
