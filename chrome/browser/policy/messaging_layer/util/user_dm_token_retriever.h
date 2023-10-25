// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_USER_DM_TOKEN_RETRIEVER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_USER_DM_TOKEN_RETRIEVER_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/dm_token_retriever.h"

namespace reporting {

// |UserDMTokenRetriever| is a |DMTokenRetriever| that is used for autonomously
// retrieving user DM tokens so it can be attached to the report queue config
// for downstream processing when building the report queue.
//
// Sample usage:
//    auto user_dm_token_retriever = UserDMTokenRetriever::Create();
//    user_dm_token_retriever->RetrieveDMToken(
//      base::BindOnce(
//        [](StatusOr<std::string> dm_token_result) {
//          config->SetDMToken(dm_token_result.value());
//        }
//      )
//    );
class UserDMTokenRetriever : public DMTokenRetriever {
 public:
  // Callback used to retrieve user profile needed for generating
  // user DM tokens
  using ProfileRetrievalCallback = base::RepeatingCallback<Profile*()>;

  // Helpers to create new retriever instances
  static std::unique_ptr<UserDMTokenRetriever> Create();
  static std::unique_ptr<UserDMTokenRetriever> CreateForTest(
      ProfileRetrievalCallback profile_retrieval_cb);

  UserDMTokenRetriever(const UserDMTokenRetriever& other) = delete;
  UserDMTokenRetriever& operator=(const UserDMTokenRetriever& other) = delete;
  ~UserDMTokenRetriever() override;

  // Retrieves user DM tokens for active user profile and triggers specified
  // callback with the corresponding result
  void RetrieveDMToken(
      DMTokenRetriever::CompletionCallback completion_cb) override;

 private:
  explicit UserDMTokenRetriever(ProfileRetrievalCallback profile_retrieval_cb);

  const ProfileRetrievalCallback profile_retrieval_cb_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_USER_DM_TOKEN_RETRIEVER_H_
