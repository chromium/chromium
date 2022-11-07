// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_info_validator.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/string_piece.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

class Profile;

namespace file_manager::trash {

namespace {

void RunCallbackWithError(base::File::Error error,
                          ValidateAndParseTrashInfoCallback callback) {
  std::move(callback).Run(base::unexpected(error));
}

}  // namespace

ParsedTrashInfoData::ParsedTrashInfoData() = default;
ParsedTrashInfoData::~ParsedTrashInfoData() = default;

ParsedTrashInfoData::ParsedTrashInfoData(ParsedTrashInfoData&& other) = default;
ParsedTrashInfoData& ParsedTrashInfoData::operator=(
    ParsedTrashInfoData&& other) = default;

TrashInfoValidator::TrashInfoValidator(Profile* profile,
                                       const base::FilePath& base_path) {
  enabled_trash_locations_ =
      trash::GenerateEnabledTrashLocationsForProfile(profile, base_path);

  parser_ = std::make_unique<chromeos::trash_service::TrashInfoParser>();
}

TrashInfoValidator::~TrashInfoValidator() = default;

void TrashInfoValidator::SetDisconnectHandler(
    base::OnceCallback<void()> disconnect_callback) {
  DCHECK(parser_) << "Parser should not be null here";
  if (parser_) {
    parser_->SetDisconnectHandler(std::move(disconnect_callback));
  }
}

void TrashInfoValidator::ValidateAndParseTrashInfo(
    const base::FilePath& trash_info_path,
    ValidateAndParseTrashInfoCallback callback) {
  // Validates the supplied file ends in a .trashinfo extension.
  if (trash_info_path.FinalExtension() != kTrashInfoExtension) {
    RunCallbackWithError(base::File::FILE_ERROR_INVALID_URL,
                         std::move(callback));
    return;
  }

  // Validate the .trashinfo file belongs in an enabled trash location.
  base::FilePath trash_folder_location;
  base::FilePath mount_point_path;
  for (const auto& [parent_path, info] : enabled_trash_locations_) {
    if (parent_path.Append(info.relative_folder_path)
            .IsParent(trash_info_path)) {
      trash_folder_location = parent_path.Append(info.relative_folder_path);
      mount_point_path = info.mount_point_path;
      break;
    }
  }

  if (mount_point_path.empty() || trash_folder_location.empty()) {
    RunCallbackWithError(base::File::FILE_ERROR_INVALID_OPERATION,
                         std::move(callback));
    return;
  }

  // Ensure the corresponding file that this metadata file refers to actually
  // exists.
  base::FilePath trashed_file_location =
      trash_folder_location.Append(kFilesFolderName)
          .Append(trash_info_path.BaseName().RemoveFinalExtension());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::PathExists, trashed_file_location),
      base::BindOnce(&TrashInfoValidator::OnTrashedFileExists,
                     weak_ptr_factory_.GetWeakPtr(), mount_point_path,
                     trashed_file_location, std::move(trash_info_path),
                     std::move(callback)));
}

void TrashInfoValidator::OnTrashedFileExists(
    const base::FilePath& mount_point_path,
    const base::FilePath& trashed_file_location,
    const base::FilePath& trash_info_path,
    ValidateAndParseTrashInfoCallback callback,
    bool exists) {
  if (!exists) {
    RunCallbackWithError(base::File::FILE_ERROR_NOT_FOUND, std::move(callback));
    return;
  }

  auto complete_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&TrashInfoValidator::OnTrashInfoParsed,
                     weak_ptr_factory_.GetWeakPtr(), trash_info_path,
                     mount_point_path, trashed_file_location,
                     std::move(callback)));

  parser_->ParseTrashInfoFile(trash_info_path, std::move(complete_callback));
}

void TrashInfoValidator::OnTrashInfoParsed(
    const base::FilePath& trash_info_path,
    const base::FilePath& mount_point_path,
    const base::FilePath& trashed_file_location,
    ValidateAndParseTrashInfoCallback callback,
    base::File::Error status,
    const base::FilePath& restore_path,
    base::Time deletion_date) {
  if (status != base::File::FILE_OK) {
    RunCallbackWithError(status, std::move(callback));
    return;
  }

  // The restore path that was parsed could be empty or not have a leading "/".
  if (restore_path.empty() ||
      restore_path.value()[0] != base::FilePath::kSeparators[0]) {
    RunCallbackWithError(base::File::FILE_ERROR_INVALID_URL,
                         std::move(callback));
    return;
  }

  // Remove the leading "/" character to make the restore path relative from the
  // known trash parent path.
  base::StringPiece relative_path =
      base::StringPiece(restore_path.value()).substr(1);
  base::FilePath absolute_restore_path = mount_point_path.Append(relative_path);

  ParsedTrashInfoData parsed_data;
  parsed_data.trash_info_path = std::move(trash_info_path);
  parsed_data.trashed_file_path = std::move(trashed_file_location);
  parsed_data.absolute_restore_path = std::move(absolute_restore_path);
  parsed_data.deletion_date = std::move(deletion_date);

  std::move(callback).Run(std::move(parsed_data));
}

}  // namespace file_manager::trash
