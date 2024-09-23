// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/nuke_profile_directory_utils.h"

#include <map>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Used in metrics for NukeProfileFromDisk(). Keep in sync with enums.xml.
//
// Entries should not be renumbered and numeric values should never be reused.
//
// Note: there are maximum 3 attempts to nuke a profile.
enum class NukeProfileResult {
  // Success values. Make sure they are consecutive.
  kSuccessFirstAttempt = 0,
  kSuccessSecondAttempt = 1,
  kSuccessThirdAttempt = 2,

  // Failure values. Make sure they are consecutive.
  kFailureFirstAttempt = 10,
  kFailureSecondAttempt = 11,
  kFailureThirdAttempt = 12,
  kMaxValue = kFailureThirdAttempt,
};

const size_t kNukeProfileMaxRetryCount = 3;

// Profile deletion can pass through two stages:
enum class ProfileDeletionStage {
  // At SCHEDULING stage some actions are made before profile deletion,
  // where one of them is the closure of browser windows. Deletion is cancelled
  // if the user choose explicitly not to close any of the tabs.
  SCHEDULING,
  // At MARKED stage profile can be safely removed from disk.
  MARKED
};

using ProfileDeletionMap = std::map<base::FilePath, ProfileDeletionStage>;
ProfileDeletionMap& ProfilesToDelete() {
  static base::NoDestructor<ProfileDeletionMap> profiles_to_delete;
  return *profiles_to_delete;
}

NukeProfileResult GetNukeProfileResult(size_t retry_count, bool success) {
  DCHECK_LT(retry_count, kNukeProfileMaxRetryCount);
  const size_t value =
      retry_count +
      static_cast<size_t>(success ? NukeProfileResult::kSuccessFirstAttempt
                                  : NukeProfileResult::kFailureFirstAttempt);
  DCHECK_LE(value, static_cast<size_t>(NukeProfileResult::kMaxValue));
  return static_cast<NukeProfileResult>(value);
}

// Implementation of NukeProfileFromDisk(), retrying at most |max_retry_count|
// times on failure. |retry_count| (initially 0) keeps track of the
// number of attempts so far.
void NukeProfileFromDiskImpl(const base::FilePath& profile_path,
                             size_t retry_count,
                             size_t max_retry_count,
                             base::OnceClosure done_callback) {
  // TODO(crbug.com/40756611): Make FileSystemProxy/FileSystemImpl expose its
  // LockTable, and/or fire events when locks are released. That way we could
  // wait for all the locks in |profile_path| to be released, rather than having
  // this retry logic.
  const base::TimeDelta kRetryDelay = base::Seconds(1);

  // Delete both the profile directory and its corresponding cache.
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(profile_path, &cache_path);

  bool success = base::DeletePathRecursively(profile_path);
  success = base::DeletePathRecursively(cache_path) && success;

  base::UmaHistogramEnumeration("Profile.NukeFromDisk.Result",
                                GetNukeProfileResult(retry_count, success));

  if (!success && retry_count < max_retry_count - 1) {
    // Failed, try again in |kRetryDelay| seconds.
    base::ThreadPool::PostDelayedTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&NukeProfileFromDiskImpl, profile_path, retry_count + 1,
                       max_retry_count, std::move(done_callback)),
        kRetryDelay);
    return;
  }

  if (done_callback) {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(done_callback));
  }
}

}  // namespace

void NukeDeletedProfilesFromDisk() {
  for (const auto& item : ProfilesToDelete()) {
    if (item.second == ProfileDeletionStage::MARKED) {
      NukeProfileFromDiskImpl(item.first, /*retry_count=*/0,
                              /*max_retry_count=*/1, base::OnceClosure());
    }
  }
  ProfilesToDelete().clear();
}

void NukeProfileFromDisk(const base::FilePath& profile_path,
                         base::OnceClosure done_callback) {
  NukeProfileFromDiskImpl(profile_path, /*retry_count=*/0,
                          kNukeProfileMaxRetryCount, std::move(done_callback));
}

bool IsProfileDirectoryMarkedForDeletion(const base::FilePath& profile_path) {
  const auto it = ProfilesToDelete().find(profile_path);
  return it != ProfilesToDelete().end() &&
         it->second == ProfileDeletionStage::MARKED;
}

void CancelProfileDeletion(const base::FilePath& path) {
  DCHECK(!base::Contains(ProfilesToDelete(), path) ||
         ProfilesToDelete()[path] == ProfileDeletionStage::SCHEDULING);
  ProfilesToDelete().erase(path);
  ProfileMetrics::LogProfileDeleteUser(ProfileMetrics::DELETE_PROFILE_ABORTED);
}

// Schedule a profile for deletion if it isn't already scheduled.
// Returns whether the profile has been newly scheduled.
bool ScheduleProfileDirectoryForDeletion(const base::FilePath& path) {
  if (base::Contains(ProfilesToDelete(), path))
    return false;
  ProfilesToDelete()[path] = ProfileDeletionStage::SCHEDULING;
  return true;
}

void MarkProfileDirectoryForDeletion(const base::FilePath& path) {
  DCHECK(!base::Contains(ProfilesToDelete(), path) ||
         ProfilesToDelete()[path] == ProfileDeletionStage::SCHEDULING);
  ProfilesToDelete()[path] = ProfileDeletionStage::MARKED;
  // Remember that this profile was deleted and files should have been deleted
  // on shutdown. In case of a crash remaining files are removed on next start.
  ScopedListPrefUpdate deleted_profiles(g_browser_process->local_state(),
                                        prefs::kProfilesDeleted);
  deleted_profiles->Append(base::FilePathToValue(path));

  // Set profile as ephemeral.
  ProfileAttributesEntry* entry = g_browser_process->profile_manager()
                                      ->GetProfileAttributesStorage()
                                      .GetProfileAttributesWithPath(path);
  if (!entry->IsEphemeral()) {
    entry->SetIsEphemeral(true);
    entry->SetIsOmitted(true);
  }
}
