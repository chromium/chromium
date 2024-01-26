// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/clear_browsing_data_job.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"

namespace enterprise_commands {

namespace {

const char kFailedTypesPath[] = "failed_data_types";

const char kClearCacheField[] = "clear_cache";
const char kClearCookiesField[] = "clear_cookies";

// Define the possibly failed data types here for 2 reasons:
//
// 1. This will be easier to keep in sync with the server, as the latter
// doesn't care about *all* the types in BrowsingDataRemover.
//
// 2. Centralize handling the underlying type of the values here.
// BrowsingDataRemover represents failed types as uint64_t, which isn't
// natively supported by base::Value, so this class needs to convert to a
// type that's supported. This will also allow us to use a list instead of a
// bit mask, which will be easier to parse gracefully on the server in case
// more types are added.
enum class DataTypes {
  kCache = 0,
  kCookies = 1,
};

std::string CreatePayload(uint64_t failed_data_types) {
  base::Value::Dict root;
  base::Value::List failed_types_list;

  if (failed_data_types & content::BrowsingDataRemover::DATA_TYPE_CACHE)
    failed_types_list.Append(static_cast<int>(DataTypes::kCache));

  if (failed_data_types & content::BrowsingDataRemover::DATA_TYPE_COOKIES)
    failed_types_list.Append(static_cast<int>(DataTypes::kCookies));

  root.Set(kFailedTypesPath, std::move(failed_types_list));

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  return payload;
}

}  // namespace

ClearBrowsingDataJob::ClearBrowsingDataJob(ProfileManager* profile_manager)
    : job_profile_picker_(profile_manager) {}
ClearBrowsingDataJob::ClearBrowsingDataJob(Profile* profile)
    : job_profile_picker_(profile) {}

ClearBrowsingDataJob::~ClearBrowsingDataJob() = default;

enterprise_management::RemoteCommand_Type ClearBrowsingDataJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA;
}

bool ClearBrowsingDataJob::ParseCommandPayload(
    const std::string& command_payload) {
  std::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root)
    return false;

  if (!root->is_dict())
    return false;
  const base::Value::Dict& dict = root->GetDict();

  if (!job_profile_picker_.ParseCommandPayload(dict)) {
    return false;
  }

  // Not specifying these fields is equivalent to setting them to false.
  clear_cache_ = dict.FindBool(kClearCacheField).value_or(false);
  clear_cookies_ = dict.FindBool(kClearCookiesField).value_or(false);

  return true;
}

void ClearBrowsingDataJob::RunImpl(CallbackWithResult result_callback) {
  uint64_t types = 0;
  if (clear_cache_)
    types |= content::BrowsingDataRemover::DATA_TYPE_CACHE;

  if (clear_cookies_)
    types |= content::BrowsingDataRemover::DATA_TYPE_COOKIES;

  Profile* profile = job_profile_picker_.GetProfile();
  if (!profile) {
    // If the payload's profile path doesn't correspond to an existing profile,
    // there's nothing to do. The most likely scenario is that the profile was
    // deleted by the time the command was received.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback), policy::ResultType::kFailure,
                       CreatePayload(types)));
    return;
  }

  result_callback_ = std::move(result_callback);

  if (types == 0) {
    // There's nothing to clear, invoke the callback with success result and be
    // done.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback_),
                                  policy::ResultType::kSuccess,
                                  CreatePayload(
                                      /*failed_data_types=*/0)));
    return;
  }

  content::BrowsingDataRemover* remover = profile->GetBrowsingDataRemover();
  remover->AddObserver(this);

  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), types,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, this);
}

void ClearBrowsingDataJob::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  Profile* profile = job_profile_picker_.GetProfile();
  DCHECK(profile);

  content::BrowsingDataRemover* remover = profile->GetBrowsingDataRemover();
  remover->RemoveObserver(this);

  std::string payload = CreatePayload(failed_data_types);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback_),
                     failed_data_types != 0 ? policy::ResultType::kFailure
                                            : policy::ResultType::kSuccess,
                     std::move(payload)));
}

}  // namespace enterprise_commands
