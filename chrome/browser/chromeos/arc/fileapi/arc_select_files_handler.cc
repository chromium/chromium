// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_select_files_handler.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "url/gurl.h"

namespace arc {

namespace {

void ContentUrlsResolved(mojom::FileSystemHost::SelectFilesCallback callback,
                         const std::vector<GURL>& content_urls) {
  mojom::SelectFilesResultPtr result = mojom::SelectFilesResult::New();
  for (const GURL& content_url : content_urls) {
    // Replace intent_helper.fileprovider with file_system.fileprovider in URL.
    // TODO(niwa): Remove this and update path_util to use
    // file_system.fileprovider by default once we complete migration.
    std::string url_string = content_url.spec();
    if (base::StartsWith(url_string, arc::kIntentHelperFileproviderUrl,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      url_string.replace(0, strlen(arc::kIntentHelperFileproviderUrl),
                         arc::kFileSystemFileproviderUrl);
    }
    result->urls.push_back(GURL(url_string));
  }
  std::move(callback).Run(std::move(result));
}

ui::SelectFileDialog::Type GetDialogType(
    const mojom::SelectFilesRequestPtr& request) {
  switch (request->action_type) {
    case mojom::SelectFilesActionType::GET_CONTENT:
    case mojom::SelectFilesActionType::OPEN_DOCUMENT:
      return request->allow_multiple
                 ? ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE
                 : ui::SelectFileDialog::SELECT_OPEN_FILE;
    case mojom::SelectFilesActionType::OPEN_DOCUMENT_TREE:
      return ui::SelectFileDialog::SELECT_EXISTING_FOLDER;
    case mojom::SelectFilesActionType::CREATE_DOCUMENT:
      return ui::SelectFileDialog::SELECT_SAVEAS_FILE;
  }
  NOTREACHED();
}

void BuildFileTypeInfo(const mojom::SelectFilesRequestPtr& request,
                       ui::SelectFileDialog::FileTypeInfo* file_type_info) {
  file_type_info->allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
  for (const std::string& mime_type : request->mime_types) {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType(mime_type, &extensions);
    file_type_info->extensions.push_back(extensions);
  }
}

}  // namespace

ArcSelectFilesHandler::ArcSelectFilesHandler(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);
}

ArcSelectFilesHandler::~ArcSelectFilesHandler() {
  // select_file_dialog_ can be nullptr only in unit tests.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

void ArcSelectFilesHandler::SelectFiles(
    const mojom::SelectFilesRequestPtr& request,
    mojom::FileSystemHost::SelectFilesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!callback_.is_null()) {
    LOG(ERROR)
        << "There is already a ui::SelectFileDialog being shown currently. "
        << "We can't open multiple ui::SelectFileDialogs at one time.";
    std::move(callback).Run(mojom::SelectFilesResult::New());
    return;
  }
  callback_ = std::move(callback);

  // TODO(niwa): Convert all request options.
  ui::SelectFileDialog::Type dialog_type = GetDialogType(request);
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  BuildFileTypeInfo(request, &file_type_info);

  select_file_dialog_->SelectFile(
      dialog_type,
      /*title=*/base::string16(),
      /*default_path=*/base::FilePath(), &file_type_info,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(),
      /*owning_window=*/nullptr,
      /*params=*/nullptr);
}

void ArcSelectFilesHandler::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<base::FilePath> files;
  files.push_back(path);
  FilesSelectedInternal(files, params);
}

void ArcSelectFilesHandler::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FilesSelectedInternal(files, params);
}

void ArcSelectFilesHandler::FileSelectionCanceled(void* params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);
  // Returns an empty result if the user cancels file selection.
  std::move(callback_).Run(mojom::SelectFilesResult::New());
}

void ArcSelectFilesHandler::FilesSelectedInternal(
    const std::vector<base::FilePath>& files,
    void* params) {
  DCHECK(callback_);

  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile_, file_manager::kFileManagerAppId);

  std::vector<storage::FileSystemURL> file_system_urls;
  for (const base::FilePath& file_path : files) {
    GURL gurl;
    file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile_, file_path, file_manager::kFileManagerAppId, &gurl);
    file_system_urls.push_back(file_system_context->CrackURL(gurl));
  }

  file_manager::util::ConvertToContentUrls(
      file_system_urls,
      base::BindOnce(&ContentUrlsResolved, std::move(callback_)));
}

void ArcSelectFilesHandler::SetSelectFileDialogForTesting(
    ui::SelectFileDialog* dialog) {
  select_file_dialog_ = dialog;
}

}  // namespace arc
