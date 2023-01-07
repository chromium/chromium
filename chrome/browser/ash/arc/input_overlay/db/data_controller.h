// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DB_DATA_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DB_DATA_CONTROLLER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "content/public/browser/browser_context.h"

namespace arc {
namespace input_overlay {

class DataController {
 public:
  DataController(content::BrowserContext& browser_cntext,
                 scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~DataController();

  // Read Proto object from file and return the Proto object for app
  // |package_name|.
  std::unique_ptr<AppDataProto> ReadProtoFromFile(
      const std::string& package_name);
  // Write the Proto object |proto| to file for app |package_name|.
  bool WriteProtoToFile(std::unique_ptr<AppDataProto> proto,
                        const std::string& package_name);

 private:
  // Create the base directory as |storage_dir_| if it doesn't exist. If it
  // returns null, the base directory didn't create successfully.
  absl::optional<base::FilePath> CreateOrGetDirectory();
  base::FilePath GetFilePathFromPackageName(const std::string& package_name);
  // Check if file |file_path| exists.
  bool ProtoFileExists(base::FilePath file_path);
  // Create empty file if file |file_path| doesn't exists.
  void CreateEmptyFile(base::FilePath file_path);

  // Base directory for GIO in the user profile.
  base::FilePath storage_dir_;
  // Task runner for the I/O functions.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DB_DATA_CONTROLLER_H_
