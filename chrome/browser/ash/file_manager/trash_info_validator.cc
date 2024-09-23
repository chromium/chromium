// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_info_validator.h"

#include <string_view>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

class Profile;

namespace file_manager::trash {

namespace {

void RunCallbackWithError(ValidationError error,
                          ValidateAndParseTrashInfoCallback callback) {
  std::move(callback).Run(base::unexpected<ValidationError>(std::move(error)));
}

}  // namespace

ParsedTrashInfoData::ParsedTrashInfoData() = default;
ParsedTrashInfoData::~ParsedTrashInfoData() = default;

ParsedTrashInfoData::ParsedTrashInfoData(ParsedTrashInfoData&& other) = default;
ParsedTrashInfoData& ParsedTrashInfoData::operator=(
    ParsedTrashInfoData&& other) = default;

std::ostream& operator<<(std::ostream& out, const ValidationError& value) {
  switch (value) {
    case ValidationError::kFileNotExist:
      out << "kFileNotExist";
      break;
    case ValidationError::kInfoNotExist:
      out << "kInfoNotExist";
      break;
    case ValidationError::kInfoFileInvalidLocation:
      out << "kInfoFileInvalidLocation";
      break;
    case ValidationError::kInfoFileInvalid:
      out << "kInfoFileInvalid";
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return out;
}

base::File::Error ValidationErrorToFileError(ValidationError error) {
  switch (error) {
    case ValidationError::kFileNotExist:
      return base::File::FILE_ERROR_NOT_FOUND;
    case ValidationError::kInfoNotExist:
      return base::File::FILE_ERROR_NOT_FOUND;
    case ValidationError::kInfoFileInvalidLocation:
      return base::File::FILE_ERROR_INVALID_URL;
    case ValidationError::kInfoFileInvalid:
      return base::File::FILE_ERROR_INVALID_OPERATION;
    default:
      NOTREACHED_IN_MIGRATION();
      return base::File::FILE_ERROR_FAILED;
  }
}

TrashInfoValidator::TrashInfoValidator(Profile* profile,
                                       const base::FilePath& base_path) {
  enabled_trash_locations_ =
      trash::GenerateEnabledTrashLocationsForProfile(profile, base_path);

  parser_ = std::make_unique<ash::trash_service::TrashInfoParser>();
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
    RunCallbackWithError(ValidationError::kInfoFileInvalid,
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
    RunCallbackWithError(ValidationError::kInfoFileInvalidLocation,
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
    RunCallbackWithError(ValidationError::kFileNotExist, std::move(callback));
    return;
  }

  auto complete_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &TrashInfoValidator::OnTrashInfoParsed, weak_ptr_factory_.GetWeakPtr(),
      trash_info_path, mount_point_path, trashed_file_location,
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
    RunCallbackWithError((status == base::File::FILE_ERROR_NOT_FOUND)
                             ? ValidationError::kInfoNotExist
                             : ValidationError::kInfoFileInvalid,
                         std::move(callback));
    return;
  }

  // The restore path that was parsed could be empty, not have a leading "/" or
  // only consist of "/".
  if (restore_path.empty() ||
      restore_path.value()[0] != base::FilePath::kSeparators[0] ||
      (restore_path.value().size() == 1 &&
       restore_path.value()[0] == base::FilePath::kSeparators[0])) {
    RunCallbackWithError(ValidationError::kInfoFileInvalid,
                         std::move(callback));
    return;
  }

  // Remove the leading "/" character to make the restore path relative from the
  // known trash parent path.
  std::string_view relative_path =
      std::string_view(restore_path.value()).substr(1);
  base::FilePath absolute_restore_path = mount_point_path.Append(relative_path);

  ParsedTrashInfoData parsed_data;
  parsed_data.trash_info_path = std::move(trash_info_path);
  parsed_data.trashed_file_path = std::move(trashed_file_location);
  parsed_data.absolute_restore_path = std::move(absolute_restore_path);
  parsed_data.deletion_date = std::move(deletion_date);

  std::move(callback).Run(
      base::ok<ParsedTrashInfoData>(std::move(parsed_data)));
}

}  // namespace file_manager::trash
