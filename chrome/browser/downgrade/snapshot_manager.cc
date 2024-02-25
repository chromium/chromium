// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/snapshot_manager.h"

#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/downgrade/downgrade_utils.h"
#include "chrome/browser/downgrade/snapshot_file_collector.h"
#include "chrome/browser/downgrade/user_data_downgrade.h"
#include "chrome/common/chrome_constants.h"

namespace downgrade {

namespace {

constexpr base::FilePath::StringPieceType kSQLiteJournalSuffix(
    FILE_PATH_LITERAL("-journal"));
constexpr base::FilePath::StringPieceType kSQLiteWalSuffix(
    FILE_PATH_LITERAL("-wal"));
constexpr base::FilePath::StringPieceType kSQLiteShmSuffix(
    FILE_PATH_LITERAL("-shm"));

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SnapshotOperationResult {
  kSuccess = 0,
  kPartialSuccess = 1,
  kFailure = 2,
  kFailedToCreateSnapshotDirectory = 3,
  kMaxValue = kFailedToCreateSnapshotDirectory
};

// Copies the item at |user_data_dir|/|relative_path| to
// |snapshot_dir|/|relative_path| if the item exists. This also copies all files
// related to items that are SQLite databases. Returns |true| if the item was
// found at the source and successfully copied. Returns |false| if the item was
// found at the source but not successfully copied. Returns no value if the file
// was not at the source.
std::optional<bool> CopyItemToSnapshotDirectory(
    const base::FilePath& relative_path,
    const base::FilePath& user_data_dir,
    const base::FilePath& snapshot_dir,
    bool is_directory) {
  const auto source = user_data_dir.Append(relative_path);
  const auto destination = snapshot_dir.Append(relative_path);

  // If nothing exists to be moved, do not consider it a success or a failure.
  if (!base::PathExists(source))
    return std::nullopt;

  bool copy_success = is_directory ? base::CopyDirectory(source, destination,
                                                         /*recursive=*/true)
                                   : base::CopyFile(source, destination);

  if (is_directory)
    return copy_success;

  // Copy SQLite journal, WAL and SHM files associated with the files that are
  // snapshotted if they exist.
  for (const auto& suffix :
       {kSQLiteJournalSuffix, kSQLiteWalSuffix, kSQLiteShmSuffix}) {
    const auto sqlite_file_path =
        base::FilePath(source.value() + base::FilePath::StringType(suffix));
    if (!base::PathExists(sqlite_file_path))
      continue;

    const auto destination_journal = base::FilePath(
        destination.value() + base::FilePath::StringType(suffix));
    copy_success &= base::CopyFile(sqlite_file_path, destination_journal);
  }

  return copy_success;
}

// Returns true if |base_name| matches a user profile directory's format. This
// function will ignore the "System Profile" directory.
bool IsProfileDir(const base::FilePath& base_name) {
  // The initial profile ("Default") is an easy one.
  if (base_name == base::FilePath().AppendASCII(chrome::kInitialProfile))
    return true;
  // Other profile dirs begin with "Profile " and end with a number.
  const base::FilePath prefix(
      base::FilePath().AppendASCII(chrome::kMultiProfileDirPrefix));
  int number;
  return base::StartsWith(base_name.value(), prefix.value(),
                          base::CompareCase::SENSITIVE) &&
         base::StringToInt(base::FilePath::StringPieceType(base_name.value())
                               .substr(prefix.value().length()),
                           &number);
}

// Returns a list of profile directory base names under |user_data_dir|.
std::vector<base::FilePath> GetUserProfileDirectories(
    const base::FilePath& user_data_dir) {
  std::vector<base::FilePath> profile_dirs;
  base::FileEnumerator enumerator(user_data_dir, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);

  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const auto base_name = path.BaseName();
    if (IsProfileDir(base_name))
      profile_dirs.push_back(std::move(base_name));
  }
  return profile_dirs;
}

// Moves the |source| directory to |target| to be deleted later. If the move
// initially fails, move the contents of the directory.
void MoveFolderForLaterDeletion(const base::FilePath& source,
                                const base::FilePath& target) {
  if (MoveWithoutFallback(source, target))
    return;
  // If some files failed to be moved, try and delete them immediately.
  if (!MoveContents(source, base::GetUniquePath(target),
                    ExclusionPredicate())) {
    base::DeletePathRecursively(source);
  }
}

}  // namespace

SnapshotManager::SnapshotManager(const base::FilePath& user_data_dir)
    : user_data_dir_(user_data_dir) {}

SnapshotManager::~SnapshotManager() = default;

void SnapshotManager::TakeSnapshot(const base::Version& version) {
  TRACE_EVENT0("browser", "SnapshotManager::TakeSnapshot");
  DCHECK(version.IsValid());
  base::FilePath snapshot_dir =
      user_data_dir_.Append(kSnapshotsDir).AppendASCII(version.GetString());

  // If the target snapshot directory already exists, try marking it for
  // deletion. In case of failure, try moving the contents then keep going.
  if (base::PathExists(snapshot_dir)) {
    auto move_target_dir = user_data_dir_.Append(kSnapshotsDir)
                               .AddExtension(kDowngradeDeleteSuffix);
    base::CreateDirectory(move_target_dir);
    // This succeeds more than 80% of the time.
    MoveFolderForLaterDeletion(
        snapshot_dir, move_target_dir.AppendASCII(version.GetString()));
  }

  auto record_item_failure = [](std::optional<bool> success,
                                SnapshotItemId id) {
    if (!success.value_or(true))
      base::UmaHistogramEnumeration("Downgrade.TakeSnapshot.ItemFailure", id);
  };

  // Abort the snapshot if the snapshot directory could not be created.
  if (!base::CreateDirectory(snapshot_dir))
    return;

  // Copy items to be preserved at the top-level of User Data.
  for (const auto& file : GetUserSnapshotItemDetails()) {
    record_item_failure(
        CopyItemToSnapshotDirectory(base::FilePath(file.path), user_data_dir_,
                                    snapshot_dir, file.is_directory),
        file.id);
  }

  const auto profile_snapshot_item_details = GetProfileSnapshotItemDetails();

  // Copy items to be preserved in each Profile directory.
  for (const auto& profile_dir : GetUserProfileDirectories(user_data_dir_)) {
    // Abort the current profile snapshot if the profile directory could not be
    // created. This succeeds almost all the time.
    if (!base::CreateDirectory(snapshot_dir.Append(profile_dir)))
      continue;
    for (const auto& file : profile_snapshot_item_details) {
      record_item_failure(CopyItemToSnapshotDirectory(
                              profile_dir.Append(file.path), user_data_dir_,
                              snapshot_dir, file.is_directory),
                          file.id);
    }
  }

  // Copy the "Last Version" file to the snapshot directory last since it is the
  // file that determines, by its presence in the snapshot directory, if the
  // snapshot is complete.
  record_item_failure(
      CopyItemToSnapshotDirectory(base::FilePath(kDowngradeLastVersionFile),
                                  user_data_dir_, snapshot_dir,
                                  /*is_directory=*/false),
      SnapshotItemId::kLastVersion);
}

void SnapshotManager::RestoreSnapshot(const base::Version& version) {
  TRACE_EVENT0("browser", "SnapshotManager::RestoreSnapshot");
  DCHECK(version.IsValid());
  auto snapshot_version = GetSnapshotToRestore(version, user_data_dir_);
  if (!snapshot_version)
    return;

  // The snapshot folder needs to be moved if it matches the current chrome
  // milestone. However, if it comes from an earlier version, it should be
  // copied so that a future downgrade to that version stays possible.
  const bool move_snapshot = snapshot_version == version;

  auto snapshot_dir = user_data_dir_.Append(kSnapshotsDir)
                          .AppendASCII(snapshot_version->GetString());

  bool has_success = false;
  bool has_error = false;
  auto record_success_error = [&has_success, &has_error](bool success) {
    has_success |= success;
    has_error |= !success;
  };
  base::FileEnumerator enumerator(
      snapshot_dir, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);

  // Move or copy the contents of the selected snapshot directory into User
  // Data.
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const auto item_info = enumerator.GetInfo();
    const auto target_path = user_data_dir_.Append(path.BaseName());
    if (move_snapshot)
      record_success_error(base::Move(path, target_path));
    else if (enumerator.GetInfo().IsDirectory())
      record_success_error(
          base::CopyDirectory(path, target_path, /*recursive=*/true));
    else
      record_success_error(base::CopyFile(path, target_path));
  }

  // When there is a partial success, according to
  // "Downgrade.RestoreSnapshot.FailureCount", the average number of items that
  // fail to be recovered is between 2 and 3.
  base::UmaHistogramEnumeration(
      "Downgrade.RestoreSnapshot.Result",
      !has_error ? SnapshotOperationResult::kSuccess
                 : has_success ? SnapshotOperationResult::kPartialSuccess
                               : SnapshotOperationResult::kFailure);

  // Mark the snapshot directory for later deletion if its contents were moved
  // into User Data. If the snapshot directory cannot be renamed, fallback to
  // moving its contents.
  if (move_snapshot) {
    auto move_target =
        GetTempDirNameForDelete(user_data_dir_, base::FilePath(kSnapshotsDir))
            .Append(snapshot_dir.BaseName());

    // Cleans up the remnants of the moved snapshot directory, this is
    // successful 99% of the time. If moving the directory fails, delete the
    // "Last Version" file so that this snapshot is considered incomplete and
    // deleted later. In case of failure to move the directory, if the Last
    // Version file is deleted, this snapshot will now be considered invalid,
    // and will be deleted, otherwise it will be overwritten at the next
    // upgrade.
    MoveFolderForLaterDeletion(snapshot_dir, move_target);

    auto last_version_file_path =
        snapshot_dir.Append(kDowngradeLastVersionFile);
    base::DeleteFile(last_version_file_path);
  }
}

void SnapshotManager::PurgeInvalidAndOldSnapshots(
    int max_number_of_snapshots,
    std::optional<uint32_t> milestone) const {
  const auto snapshot_dir = user_data_dir_.Append(kSnapshotsDir);

  // Move the invalid snapshots within from Snapshots/NN to Snapshots.DELETE/NN.
  const base::FilePath target =
      snapshot_dir.AddExtension(kDowngradeDeleteSuffix);
  base::CreateDirectory(target);

  // Moves all the invalid snapshots for later deletion.
  auto invalid_snapshots = GetInvalidSnapshots(snapshot_dir);
  for (const auto& path : invalid_snapshots) {
    // This succeeds 97% of the time according to
    // Downgrade.InvalidSnapshotMove.Result, with most of the failures having
    // under 4 files failing to be copied.
    MoveFolderForLaterDeletion(path, target.Append(path.BaseName()));
  }

  base::flat_set<base::Version> available_snapshots =
      GetAvailableSnapshots(snapshot_dir);
  if (milestone.has_value()) {
    // Only consider versions for the specified milestone.
    available_snapshots.erase(available_snapshots.upper_bound(
                                  base::Version({*milestone + 1, 0, 0, 0})),
                              available_snapshots.end());
    available_snapshots.erase(
        available_snapshots.begin(),
        available_snapshots.lower_bound(base::Version({*milestone, 0, 0, 0})));
  }

  if (available_snapshots.size() <=
      base::checked_cast<size_t>(max_number_of_snapshots)) {
    return;
  }

  size_t number_of_snapshots_to_delete =
      available_snapshots.size() - max_number_of_snapshots;

  // Moves all the older snapshots for later deletion.
  for (const auto& snapshot : available_snapshots) {
    auto snapshot_path = snapshot_dir.AppendASCII(snapshot.GetString());
    // This succeeds 97% of the time according to
    // Downgrade.InvalidSnapshotMove.Result, with most of the failures having
    // under 4 files failing to be copied.
    MoveFolderForLaterDeletion(snapshot_path,
                               target.Append(snapshot_path.BaseName()));
    if (--number_of_snapshots_to_delete == 0)
      break;
  }
}

void SnapshotManager::DeleteSnapshotDataForProfile(
    base::Time delete_begin,
    const base::FilePath& profile_base_name,
    uint64_t remove_mask) {
  using chrome_browsing_data_remover::ALL_DATA_TYPES;
  using chrome_browsing_data_remover::WIPE_PROFILE;

  bool delete_all = (((remove_mask & WIPE_PROFILE) == WIPE_PROFILE) ||
                     ((remove_mask & ALL_DATA_TYPES) == ALL_DATA_TYPES)) &&
                    delete_begin.is_null();
  std::vector<base::FilePath> files_to_delete;
  if (!delete_all) {
    for (const auto& item : CollectProfileItems()) {
      if (item.data_types & remove_mask)
        files_to_delete.push_back(item.path);
    }
  }

  if (!delete_all && files_to_delete.empty())
    return;

  const auto snapshot_dir = user_data_dir_.Append(kSnapshotsDir);
  auto available_snapshots = GetAvailableSnapshots(snapshot_dir);

  base::File::Info file_info;
  for (const auto& snapshot : available_snapshots) {
    auto snapshot_path = snapshot_dir.AppendASCII(snapshot.GetString());
    // If we are not able to get the file info, it probably has been deleted.
    if (!base::GetFileInfo(snapshot_path, &file_info))
      continue;
    auto profile_absolute_path = snapshot_path.Append(profile_base_name);
    // Deletes the whole profile from the snapshots if it is being wiped
    // regardless of |delete_begin|, otherwise deletes the required files from
    // the snapshot if it was created after |delete_begin|.
    if (delete_all) {
      base::DeletePathRecursively(profile_absolute_path);
    } else if (delete_begin <= file_info.creation_time &&
               base::PathExists(profile_absolute_path)) {
      for (const auto& filename : files_to_delete) {
        base::DeletePathRecursively(profile_absolute_path.Append(filename));
      }
      // Non recursive deletion will fail if the directory is not empty. In this
      // case we only want to delete the directory if it is empty.
      base::DeleteFile(profile_absolute_path);
    }
  }
}

std::vector<SnapshotItemDetails>
SnapshotManager::GetProfileSnapshotItemDetails() const {
  return CollectProfileItems();
}

std::vector<SnapshotItemDetails> SnapshotManager::GetUserSnapshotItemDetails()
    const {
  return CollectUserDataItems();
}

}  // namespace downgrade
