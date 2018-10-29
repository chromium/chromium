// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/arc/common/file_system.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

// Handler for FileSystemHost.SelectFiles.
class ArcSelectFilesHandler : public ui::SelectFileDialog::Listener {
 public:
  explicit ArcSelectFilesHandler(content::BrowserContext* context);
  ~ArcSelectFilesHandler() override;

  void SelectFiles(const mojom::SelectFilesRequestPtr& request,
                   mojom::FileSystemHost::SelectFilesCallback callback);

  // ui::SelectFileDialog::Listener overrides:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

  void SetSelectFileDialogForTesting(ui::SelectFileDialog* dialog);

 private:
  friend class ArcSelectFilesHandlerTest;

  void FilesSelectedInternal(const std::vector<base::FilePath>& files,
                             void* params);

  Profile* const profile_;

  mojom::FileSystemHost::SelectFilesCallback callback_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ArcSelectFilesHandler);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
