// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_ANDROID_FAMILY_INFO_FEEDBACK_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_ANDROID_FAMILY_INFO_FEEDBACK_SOURCE_H_

#include <map>
#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher_status.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"

class Profile;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace chrome::android {

// Native class for Java counterpart. Retrieves family information
// asynchronously.
class FamilyInfoFeedbackSource {
 public:
  FamilyInfoFeedbackSource(const base::android::JavaParamRef<jobject>& obj,
                           Profile* profile);

  FamilyInfoFeedbackSource(const FamilyInfoFeedbackSource&) = delete;
  FamilyInfoFeedbackSource& operator=(const FamilyInfoFeedbackSource&) = delete;
  ~FamilyInfoFeedbackSource();

  // Retrieves a list of family members for the primary account.
  void GetFamilyMembers();

 private:
  friend class FamilyInfoFeedbackSourceTest;
  friend class FamilyInfoFeedbackSourceForChildFilterBehaviorTest;

  void OnResponse(
      const supervised_user::ProtoFetcherStatus& status,
      std::unique_ptr<kidsmanagement::ListMembersResponse> response);
  void OnSuccess(const kidsmanagement::ListMembersResponse& response);
  void OnFailure(const supervised_user::ProtoFetcherStatus& status);

  // Cleans up following the call to ListFamilyMembers
  void OnComplete();

  raw_ptr<supervised_user::SupervisedUserService> supervised_user_service_;
  std::unique_ptr<supervised_user::ListFamilyMembersFetcher>
      list_family_members_fetcher_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  base::WeakPtrFactory<FamilyInfoFeedbackSource> weak_factory_{this};
};

}  // namespace chrome::android

#endif  // CHROME_BROWSER_FEEDBACK_ANDROID_FAMILY_INFO_FEEDBACK_SOURCE_H_
