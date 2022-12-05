// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_INFO_VALIDATOR_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_INFO_VALIDATOR_H_

#include <utility>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chromeos/ash/components/trash_service/public/cpp/trash_info_parser.h"

namespace file_manager::trash {

// On a successful parse of .trashinfo files, returns the restoration path,
// deletion date and actual location of the trashed file.
struct ParsedTrashInfoData {
  ParsedTrashInfoData();
  ~ParsedTrashInfoData();

  ParsedTrashInfoData(ParsedTrashInfoData&& other);
  ParsedTrashInfoData& operator=(ParsedTrashInfoData&& other);

  // The on-disk location of the .trashinfo file, e.g.
  // .Trash/info/foo.txt.trashinfo.
  base::FilePath trash_info_path;

  // The actual on-disk location of the trashed file, e.g. .Trash/files/foo.txt.
  base::FilePath trashed_file_path;

  // The path to restore the file back to. The basename here and the basename
  // for the `trashed_file_path` may differ as the path may conflict with
  // another in the trash.
  base::FilePath absolute_restore_path;

  // The date/time the file was trashed.
  base::Time deletion_date;
};

// Possible validation errors that may arise.
enum class ValidationError {
  kFileNotExist = 0,
  kInfoNotExist = 1,
  kInfoFileInvalid = 2,
  kInfoFileInvalidLocation = 3,
};

// Helper operator to enable pretty printing of the validation errors to logs.
std::ostream& operator<<(std::ostream& out, const ValidationError& value);

// Helper to convert the underlying `ValidationError` to a base::File::Error.
base::File::Error ValidationErrorToFileError(ValidationError error);

// Helper alias to define the callback type that is returned from the validator.
using ParsedTrashInfoDataOrError =
    base::expected<ParsedTrashInfoData, ValidationError>;
using ValidateAndParseTrashInfoCallback =
    base::OnceCallback<void(ParsedTrashInfoDataOrError)>;

// Validates and parses individual .trashinfo files to ensure they conform to
// the XDG specification. This is exposed here as we need to get a file handler
// and ensure files exist prior to parsing them. This can be done in a
// privileged context, but the parsing cannot. To use this:
//   1. Initialize a `TrashInfoValidator` like:
//        auto parser = std::make_unique<TrashInfoValidator>(profile,
//          base_path);
//   2. Set your disconnect handler in the event the underlying trash service
//   disconnects errorenously.
//        parser->SetDisconnectHandler(base::BindOnce(&Method, WeakPtr()));
//   3. For every file to validate and parse, call the ValidateAndParseTrashInfo
//   method, e.g.
//        parser->ValidateAndParseTrashInfo(trash_info_path,
//          base::BindOnce(&OnParsed, WeakPtr()));
class TrashInfoValidator {
 public:
  // The `base_path` here is used primarily for testing purposes to identify the
  // enabled trash locations.
  TrashInfoValidator(Profile* profile, const base::FilePath& base_path);
  ~TrashInfoValidator();

  TrashInfoValidator(const TrashInfoValidator&) = delete;
  TrashInfoValidator& operator=(const TrashInfoValidator&) = delete;

  // Ensure the metadata file conforms to the following:
  //   - Has a .trashinfo suffix
  //   - Resides in an enabled trash directory
  //   - The file resides in the info directory
  //   - Has an identical item in the files directory with no .trashinfo suffix
  // In the event the above fails, the `callback` will be invoked with an error,
  // on success it then calls the TrashService to retrieve the parsed trashinfo
  // data. The `trash_info_path` must be absolute.
  void ValidateAndParseTrashInfo(const base::FilePath& trash_info_path,
                                 ValidateAndParseTrashInfoCallback callback);

  // Set the disconnect handler for the underlying TrashService.
  // TODO(b/238946031): Potentially centralize this by calling the `callback`
  // instead of having a separate disconnect callback.
  void SetDisconnectHandler(base::OnceCallback<void()> disconnect_callback);

 private:
  // Invoked after verifying if the on-disk file exists. The `mount_point_path`
  // represents the location where the .Trash folder resides (e.g. ~/MyFiles),
  // the `trashed_file_location` is the on-disk file that should be restored and
  // the `trash_info_path` represents the location of the .trashinfo file.
  void OnTrashedFileExists(const base::FilePath& mount_point_path,
                           const base::FilePath& trashed_file_location,
                           const base::FilePath& trash_info_path,
                           ValidateAndParseTrashInfoCallback callback,
                           bool exists);

  // Invoked when the TrashService has finished parsing the .trashinfo file.
  void OnTrashInfoParsed(const base::FilePath& trash_info_path,
                         const base::FilePath& mount_point_path,
                         const base::FilePath& trashed_file_location,
                         ValidateAndParseTrashInfoCallback callback,
                         base::File::Error status,
                         const base::FilePath& restore_path,
                         base::Time deletion_date);

  // A map containing paths which are enabled for trashing.
  trash::TrashPathsMap enabled_trash_locations_;

  // Holds the connection open to the `TrashService`. This is a sandboxed
  // process that performs parsing of the trashinfo files.
  std::unique_ptr<ash::trash_service::TrashInfoParser> parser_ = nullptr;

  base::WeakPtrFactory<TrashInfoValidator> weak_ptr_factory_{this};
};

}  // namespace file_manager::trash

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_INFO_VALIDATOR_H_
