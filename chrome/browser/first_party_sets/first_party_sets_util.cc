// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_util.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_party_sets/first_party_sets_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "net/base/features.h"

namespace {

constexpr base::FilePath::CharType kPersistedFirstPartySetsFileName[] =
    FILE_PATH_LITERAL("persisted_first_party_sets.json");

// Reads the sets as raw JSON from their storage file, returning the raw sets on
// success and empty string on failure.
std::string LoadSetsFromDisk(const base::FilePath& path) {
  DCHECK(!path.empty());

  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    VLOG(1) << "Failed loading serialized First-Party Sets file from "
            << path.MaybeAsASCII();
    return "";
  }
  return result;
}

// Writes the sets as raw JSON to the storage file.
//
// TODO(crbug.com/1219656): To handle the cases of file corrupting due to
// incomplete writes, write to a temp file then rename over the old file.
void MaybeWriteSetsToDisk(const base::FilePath& path, const std::string& sets) {
  DCHECK(!path.empty());

  if (!base::WriteFile(path, sets)) {
    VLOG(1) << "Failed writing serialized First-Party Sets to file "
            << path.MaybeAsASCII();
  }
}

bool IsFirstPartySetsEnabledInternal() {
  if (!base::FeatureList::IsEnabled(features::kFirstPartySets)) {
    return false;
  }
  if (!g_browser_process) {
    // If browser process doesn't exist (e.g. in minimal mode on android),
    // default to the feature value which is true since we didn't return above.
    return true;
  }
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state ||
      !local_state->FindPreference(first_party_sets::kFirstPartySetsEnabled)) {
    return true;
  }
  return local_state->GetBoolean(first_party_sets::kFirstPartySetsEnabled);
}

}  // namespace

// static
FirstPartySetsUtil* FirstPartySetsUtil::GetInstance() {
  static base::NoDestructor<FirstPartySetsUtil> instance;
  return instance.get();
}

FirstPartySetsUtil::FirstPartySetsUtil() = default;

void FirstPartySetsUtil::SendAndUpdatePersistedSets(
    const base::FilePath& user_data_dir,
    base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                            const std::string&)> send_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!user_data_dir.empty());
  const base::FilePath persisted_sets_path =
      user_data_dir.Append(kPersistedFirstPartySetsFileName);

  // base::Unretained(this) is safe here because this is a static singleton.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadSetsFromDisk, persisted_sets_path),
      base::BindOnce(&FirstPartySetsUtil::SendPersistedSets,
                     base::Unretained(this), std::move(send_sets),
                     persisted_sets_path));
}

bool FirstPartySetsUtil::IsFirstPartySetsEnabled() {
  // This method invokes the private IsFirstPartySetsEnabledInternal method
  // and uses the `enabled_` variable to memoize the result. We can memoize
  // since the First-Party Sets enterprise policy doesn't support `dynamic
  // refresh` and the base::Feature doesn't change after start up, the value
  // of this method will not change in the same in any browser session.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enabled_.has_value()) {
    enabled_ = absl::make_optional(IsFirstPartySetsEnabledInternal());
  }
  return enabled_.value();
}

void FirstPartySetsUtil::OnGetUpdatedSets(const base::FilePath& path,
                                          const std::string& sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&MaybeWriteSetsToDisk, path, sets));
}

void FirstPartySetsUtil::SendPersistedSets(
    base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                            const std::string&)> send_sets,
    const base::FilePath& path,
    const std::string& sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // base::Unretained(this) is safe here because this is a static singleton.
  std::move(send_sets).Run(base::BindOnce(&FirstPartySetsUtil::OnGetUpdatedSets,
                                          base::Unretained(this), path),
                           sets);
}
