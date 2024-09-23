// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/android/family_info_feedback_source.h"

#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/feedback/android/jni_headers/FamilyInfoFeedbackSource_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace chrome::android {

void JNI_FamilyInfoFeedbackSource_Start(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        Profile* profile) {
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
  list_family_members_fetcher_ = FetchListFamilyMembers(
      *identity_manager_, url_loader_factory_,
      base::BindOnce(
          &FamilyInfoFeedbackSource::OnResponse,
          base::Unretained(this)));  // Unretained(.) is safe because `this`
                                     // owns `list_family_members_fetcher_`.
}

void FamilyInfoFeedbackSource::OnResponse(
    const supervised_user::ProtoFetcherStatus& status,
    std::unique_ptr<kidsmanagement::ListMembersResponse> response) {
  if (!status.IsOk()) {
    OnFailure(status);
    return;
  }
  OnSuccess(*response);
  // Release response.
}

void FamilyInfoFeedbackSource::OnSuccess(
    const kidsmanagement::ListMembersResponse& response) {
  std::string primary_account_gaia =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  JNIEnv* env = AttachCurrentThread();
  for (const kidsmanagement::FamilyMember& member : response.members()) {
    // Store the family member role for the primary account of the profile.
    if (primary_account_gaia == member.user_id()) {
      // If a child is signed-in, report the parental control web filter.
      ScopedJavaLocalRef<jstring> child_web_filter_type = nullptr;
      if (member.role() == kidsmanagement::CHILD) {
        supervised_user::WebFilterType web_filter_type =
            supervised_user_service_->GetURLFilter()->GetWebFilterType();
        child_web_filter_type = ConvertUTF8ToJavaString(
            env,
            supervised_user::WebFilterTypeToDisplayString(web_filter_type));
      }
      Java_FamilyInfoFeedbackSource_processPrimaryAccountFamilyInfo(
          env, java_ref_,
          ConvertUTF8ToJavaString(
              env, supervised_user::FamilyRoleToString(member.role())),
          child_web_filter_type);
    }
  }
  OnComplete();
}

void FamilyInfoFeedbackSource::OnFailure(
    const supervised_user::ProtoFetcherStatus& status) {
  DLOG(WARNING) << "ListFamilyMembers failed with status: "
                << status.ToString();
  OnComplete();
}

void FamilyInfoFeedbackSource::OnComplete() {
  // Object will delete itself following the fetch to ListFamilyMembers.
  delete this;
}

}  // namespace chrome::android
