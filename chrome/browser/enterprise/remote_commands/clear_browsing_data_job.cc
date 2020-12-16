// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/clear_browsing_data_job.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace enterprise_commands {

const char kFailedTypesPath[] = "failed_data_types";

const char kProfilePathField[] = "profile_path";
const char kClearCacheField[] = "clear_cache";
const char kClearCookiesField[] = "clear_cookies";

ClearBrowsingDataJob::ResultPayload::ResultPayload(uint64_t failed_data_types)
    : failed_data_types_(failed_data_types) {}

ClearBrowsingDataJob::ResultPayload::~ResultPayload() = default;

std::unique_ptr<std::string> ClearBrowsingDataJob::ResultPayload::Serialize() {
  base::Value root(base::Value::Type::DICTIONARY);
  base::Value failed_types_list(base::Value::Type::LIST);

  if (failed_data_types_ & content::BrowsingDataRemover::DATA_TYPE_CACHE)
    failed_types_list.Append(static_cast<int>(CACHE));

  if (failed_data_types_ & content::BrowsingDataRemover::DATA_TYPE_COOKIES)
    failed_types_list.Append(static_cast<int>(COOKIES));

  root.SetPath(kFailedTypesPath, std::move(failed_types_list));

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  return std::make_unique<std::string>(std::move(payload));
}

ClearBrowsingDataJob::ClearBrowsingDataJob(ProfileManager* profile_manager)
    : profile_manager_(profile_manager) {}

ClearBrowsingDataJob::~ClearBrowsingDataJob() = default;

enterprise_management::RemoteCommand_Type ClearBrowsingDataJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA;
}

bool ClearBrowsingDataJob::ParseCommandPayload(
    const std::string& command_payload) {
  base::Optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root)
    return false;

  if (!root->is_dict())
    return false;

  std::string* path = root->FindStringKey(kProfilePathField);
  if (!path)
    return false;

  // On Windows, file paths are wstring as opposed to string on other platforms.
  // On POSIX platforms other than MacOS and ChromeOS, the encoding is unknown.
  //
  // This path is sent from the server, which obtained it from Chrome in a
  // previous report, and Chrome casts the path as UTF8 using UTF8Unsafe before
  // sending it (see BrowserReportGeneratorDesktop::GenerateProfileInfo).
  // Because of that, the best thing we can do everywhere is try to get the
  // path from UTF8, and ending up with an invalid path will fail later in
  // RunImpl when we attempt to get the profile from the path.
  profile_path_ = base::FilePath::FromUTF8Unsafe(*path);
#if defined(OS_WIN)
  // For Windows machines, the path that Chrome reports for the profile is
  // "Normalized" to all lower-case on the reporting server. This means that
  // when the server sends the command, the path will be all lower case and
  // the profile manager won't be able to use it as a key. To avoid this issue,
  // This code will iterate over all profile paths and find the one that matches
  // in a case-insensitive comparison. If this doesn't find one, RunImpl will
  // fail in the same manner as if the profile didn't exist, which is the
  // expected behavior.
  ProfileAttributesStorage& storage =
      profile_manager_->GetProfileAttributesStorage();
  for (ProfileAttributesEntry* entry : storage.GetAllProfilesAttributes()) {
    base::FilePath entry_path = entry->GetPath();

    if (base::FilePath::CompareEqualIgnoreCase(profile_path_.value(),
                                               entry_path.value())) {
      profile_path_ = entry_path;
      break;
    }
  }
#endif

  // Not specifying these fields is equivalent to setting them to false.
  clear_cache_ = root->FindBoolKey(kClearCacheField).value_or(false);
  clear_cookies_ = root->FindBoolKey(kClearCookiesField).value_or(false);

  return true;
}

void ClearBrowsingDataJob::RunImpl(CallbackWithResult succeeded_callback,
                                   CallbackWithResult failed_callback) {
  DCHECK(profile_manager_);

  uint64_t types = 0;
  if (clear_cache_)
    types |= content::BrowsingDataRemover::DATA_TYPE_CACHE;

  if (clear_cookies_)
    types |= content::BrowsingDataRemover::DATA_TYPE_COOKIES;

  Profile* profile = profile_manager_->GetProfileByPath(profile_path_);
  if (!profile) {
    // If the payload's profile path doesn't correspond to an existing profile,
    // there's nothing to do. The most likely scenario is that the profile was
    // deleted by the time the command was received.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(failed_callback),
                                  std::make_unique<ResultPayload>(types)));
    return;
  }

  succeeded_callback_ = std::move(succeeded_callback);
  failed_callback_ = std::move(failed_callback);

  if (types == 0) {
    // There's nothing to clear, invoke the success callback and be done.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(succeeded_callback_),
                                  std::make_unique<ResultPayload>(
                                      /* failed_types= */ 0)));
    return;
  }

  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(profile);
  remover->AddObserver(this);

  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), types,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, this);
}

void ClearBrowsingDataJob::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  Profile* profile = profile_manager_->GetProfileByPath(profile_path_);
  DCHECK(profile);

  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(profile);
  remover->RemoveObserver(this);

  auto payload = std::make_unique<ResultPayload>(failed_data_types);

  if (failed_data_types != 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(failed_callback_), std::move(payload)));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(succeeded_callback_), std::move(payload)));
  }
}

}  // namespace enterprise_commands
