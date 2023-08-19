// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DB_DATA_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DB_DATA_CONTROLLER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "content/public/browser/browser_context.h"

namespace arc::input_overlay {

class DataController {
 public:
  DataController(content::BrowserContext& browser_cntext,
                 scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~DataController();

  // Read Proto object from file and return the Proto object for app
  // `file_path`.
  static std::unique_ptr<AppDataProto> ReadProtoFromFile(
      base::FilePath file_path);
  // Write the Proto object `proto` to file for app `file_path`.
  static bool WriteProtoToFile(std::unique_ptr<AppDataProto> proto,
                               base::FilePath file_path);

  base::FilePath GetFilePathFromPackageName(const std::string& package_name);

 private:
  // Base directory for GIO in the user profile.
  base::FilePath storage_dir_;
  // Task runner for the I/O functions.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DB_DATA_CONTROLLER_H_
