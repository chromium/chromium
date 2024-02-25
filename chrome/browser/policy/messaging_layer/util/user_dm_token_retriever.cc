// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/user_dm_token_retriever.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/dm_token_retriever.h"
#include "components/reporting/util/statusor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

namespace {

// Retrieves and returns the user DM token for the profile specified by the
// profile delegate
policy::DMToken GetDMToken(
    const UserDMTokenRetriever::ProfileRetrievalCallback profile_retrieval_cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Profile* profile = std::move(profile_retrieval_cb).Run();
  policy::DMToken dm_token = policy::GetDMToken(profile);
  return dm_token;
}

// Gets current active user profile
Profile* GetUserProfile() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(IS_CHROMEOS)
  return ProfileManager::GetActiveUserProfile();
#else
  return ProfileManager::GetLastUsedProfile();
#endif
}

// Processes retrieved DM token even if the retriever goes out of scope in the
// caller.
void OnDMTokenRetrieved(DMTokenRetriever::CompletionCallback completion_cb,
                        const policy::DMToken& dm_token) {
  // Return an error if DM token is invalid
  if (!dm_token.is_valid()) {
    std::move(completion_cb)
        .Run(base::unexpected(
            Status(error::UNKNOWN, "Invalid DM token received")));
    return;
  }

  std::move(completion_cb).Run(dm_token.value());
}

}  // namespace

UserDMTokenRetriever::UserDMTokenRetriever(
    UserDMTokenRetriever::ProfileRetrievalCallback profile_retrieval_cb)
    : profile_retrieval_cb_(std::move(profile_retrieval_cb)) {}

UserDMTokenRetriever::~UserDMTokenRetriever() = default;

// static
std::unique_ptr<UserDMTokenRetriever> UserDMTokenRetriever::Create() {
  const ProfileRetrievalCallback profile_retrieval_cb =
      base::BindRepeating(&GetUserProfile);
  return base::WrapUnique(
      new UserDMTokenRetriever(std::move(profile_retrieval_cb)));
}

// static
std::unique_ptr<UserDMTokenRetriever> UserDMTokenRetriever::CreateForTest(
    UserDMTokenRetriever::ProfileRetrievalCallback profile_retrieval_cb) {
  return base::WrapUnique(
      new UserDMTokenRetriever(std::move(profile_retrieval_cb)));
}

// Gets user DM token for current active user
void UserDMTokenRetriever::RetrieveDMToken(
    DMTokenRetriever::CompletionCallback completion_cb) {
  // DM token needs to be retrieved on the UI thread, so we retrieve the token
  // on the UI thread and process the result in the regular thread.
  //
  // TODO: (b/203616986) This is an expensive operation, and DM token should
  // ideally be be cached, but needs to be done meticulously accounting for
  // cache coherency.
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetDMToken, profile_retrieval_cb_),
      base::BindOnce(&OnDMTokenRetrieved, std::move(completion_cb)));
}

}  // namespace reporting
