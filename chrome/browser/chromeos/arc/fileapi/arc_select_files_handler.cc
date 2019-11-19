// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_select_files_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_select_files_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/arc/arc_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace arc {

// Script for clicking OK button on the selector.
const char kScriptClickOk[] =
    "(function() { document.querySelector('#ok-button').click(); })();";

// Script for clicking Cancel button on the selector.
const char kScriptClickCancel[] =
    "(function() { document.querySelector('#cancel-button').click(); })();";

// Script for clicking a directory element in the left pane of the selector.
// %s should be replaced by the target directory name wrapped by double-quotes.
const char kScriptClickDirectory[] =
    "(function() {"
    "  var dirs = document.querySelectorAll('#directory-tree .entry-name');"
    "  Array.from(dirs).filter(a => a.innerText === %s)[0].click();"
    "})();";

// Script for clicking a file element in the right pane of the selector.
// %s should be replaced by the target file name wrapped by double-quotes.
const char kScriptClickFile[] =
    "(function() {"
    "  var evt = document.createEvent('MouseEvents');"
    "  evt.initMouseEvent('mousedown', true, false);"
    "  var files = document.querySelectorAll('#file-list .file');"
    "  Array.from(files).filter(a => a.getAttribute('file-name') === %s)[0]"
    "      .dispatchEvent(evt);"
    "})();";

// Script for querying UI elements (directories and files) shown on the selector.
const char kScriptGetElements[] =
    "(function() {"
    "  var dirs = document.querySelectorAll('#directory-tree .entry-name');"
    "  var files = document.querySelectorAll('#file-list .file');"
    "  return {dirNames: Array.from(dirs, a => a.innerText),"
    "          fileNames: Array.from(files, a => a.getAttribute('file-name'))};"
    "})();";

namespace {

void ConvertToElementVector(
    const base::Value* list_value,
    std::vector<mojom::FileSelectorElementPtr>* elements) {
  if (!list_value || !list_value->is_list())
    return;

  for (const base::Value& value : list_value->GetList()) {
    mojom::FileSelectorElementPtr element = mojom::FileSelectorElement::New();
    element->name = value.GetString();
    elements->push_back(std::move(element));
  }
}

void OnGetElementsScriptResults(
    mojom::FileSystemHost::GetFileSelectorElementsCallback callback,
    base::Value value) {
  mojom::FileSelectorElementsPtr result = mojom::FileSelectorElements::New();
  if (value.is_dict()) {
    ConvertToElementVector(value.FindKey("dirNames"),
                           &result->directory_elements);
    ConvertToElementVector(value.FindKey("fileNames"), &result->file_elements);
  }
  std::move(callback).Run(std::move(result));
}

void ContentUrlsResolved(mojom::FileSystemHost::SelectFilesCallback callback,
                         const std::vector<GURL>& content_urls) {
  mojom::SelectFilesResultPtr result = mojom::SelectFilesResult::New();
  for (const GURL& content_url : content_urls) {
    result->urls.push_back(content_url);
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

base::FilePath GetInitialFilePath(const mojom::SelectFilesRequestPtr& request) {
  const mojom::DocumentPathPtr& document_path = request->initial_document_path;
  if (!document_path)
    return base::FilePath();

  if (document_path->path.empty()) {
    LOG(ERROR) << "path should at least contain root Document ID.";
    return base::FilePath();
  }

  const std::string& root_document_id = document_path->path[0];
  // TODO(niwa): Convert non-root document IDs to the relative path and append.
  return arc::GetDocumentsProviderMountPath(document_path->authority,
                                            root_document_id);
}

void BuildFileTypeInfo(const mojom::SelectFilesRequestPtr& request,
                       ui::SelectFileDialog::FileTypeInfo* file_type_info) {
  file_type_info->allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
  for (const std::string& mime_type : request->mime_types) {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType(mime_type, &extensions);
    if (extensions.empty()) {
      // Allow the user to select all files if MIME type conversion fails.
      file_type_info->include_all_files = true;
    } else {
      file_type_info->extensions.push_back(extensions);
    }
  }
}

}  // namespace

ArcSelectFilesHandlersManager::ArcSelectFilesHandlersManager(
    content::BrowserContext* context)
    : context_(context) {}

ArcSelectFilesHandlersManager::~ArcSelectFilesHandlersManager() = default;

void ArcSelectFilesHandlersManager::SelectFiles(
    const mojom::SelectFilesRequestPtr& request,
    mojom::FileSystemHost::SelectFilesCallback callback) {
  int task_id = request->task_id;
  if (handlers_by_task_id_.find(task_id) != handlers_by_task_id_.end()) {
    LOG(ERROR) << "SelectFileDialog is already shown for task ID : " << task_id;
    std::move(callback).Run(mojom::SelectFilesResult::New());
    return;
  }

  auto handler = std::make_unique<ArcSelectFilesHandler>(context_);
  auto* handler_ptr = handler.get();
  handlers_by_task_id_.emplace(task_id, std::move(handler));

  // Make sure that the handler is erased when the SelectFileDialog is closed.
  handler_ptr->SelectFiles(
      std::move(request),
      base::BindOnce(&ArcSelectFilesHandlersManager::EraseHandlerAndRunCallback,
                     weak_ptr_factory_.GetWeakPtr(), task_id,
                     std::move(callback)));
}

void ArcSelectFilesHandlersManager::OnFileSelectorEvent(
    mojom::FileSelectorEventPtr event,
    mojom::FileSystemHost::OnFileSelectorEventCallback callback) {
  int task_id = event->creator_task_id;
  auto iter = handlers_by_task_id_.find(task_id);
  if (iter == handlers_by_task_id_.end()) {
    LOG(ERROR) << "Can't find a SelectFileDialog for task ID : " << task_id;
    std::move(callback).Run();
    return;
  }
  iter->second->OnFileSelectorEvent(std::move(event), std::move(callback));
}

void ArcSelectFilesHandlersManager::GetFileSelectorElements(
    mojom::GetFileSelectorElementsRequestPtr request,
    mojom::FileSystemHost::GetFileSelectorElementsCallback callback) {
  int task_id = request->creator_task_id;
  auto iter = handlers_by_task_id_.find(task_id);
  if (iter == handlers_by_task_id_.end()) {
    LOG(ERROR) << "Can't find a SelectFileDialog for task ID : " << task_id;
    std::move(callback).Run(mojom::FileSelectorElements::New());
    return;
  }
  iter->second->GetFileSelectorElements(std::move(request),
                                        std::move(callback));
}

void ArcSelectFilesHandlersManager::EraseHandlerAndRunCallback(
    int task_id,
    mojom::FileSystemHost::SelectFilesCallback callback,
    mojom::SelectFilesResultPtr result) {
  handlers_by_task_id_.erase(task_id);
  std::move(callback).Run(std::move(result));
}

ArcSelectFilesHandler::ArcSelectFilesHandler(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  dialog_holder_ = std::make_unique<SelectFileDialogHolder>(this);
}

ArcSelectFilesHandler::~ArcSelectFilesHandler() {
  // Make sure to close SelectFileDialog when the handler is destroyed.
  dialog_holder_->ExecuteJavaScript(kScriptClickCancel, {});
}

void ArcSelectFilesHandler::SelectFiles(
    const mojom::SelectFilesRequestPtr& request,
    mojom::FileSystemHost::SelectFilesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  callback_ = std::move(callback);

  // TODO(niwa): Convert all request options.
  ui::SelectFileDialog::Type dialog_type = GetDialogType(request);
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  BuildFileTypeInfo(request, &file_type_info);
  base::FilePath default_path = GetInitialFilePath(request);

  // Android picker apps should be shown in GET_CONTENT mode.
  bool show_android_picker_apps =
      request->action_type == mojom::SelectFilesActionType::GET_CONTENT;

  bool success =
      dialog_holder_->SelectFile(dialog_type, default_path, &file_type_info,
                                 request->task_id, show_android_picker_apps);
  if (!success) {
    std::move(callback_).Run(mojom::SelectFilesResult::New());
  }
}

void ArcSelectFilesHandler::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);

  const std::string& activity = ConvertFilePathToAndroidActivity(path);
  if (!activity.empty()) {
    // The user selected an Android picker activity instead of a file.
    mojom::SelectFilesResultPtr result = mojom::SelectFilesResult::New();
    result->picker_activity = activity;
    std::move(callback_).Run(std::move(result));
    return;
  }

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

void ArcSelectFilesHandler::OnFileSelectorEvent(
    mojom::FileSelectorEventPtr event,
    mojom::FileSystemHost::OnFileSelectorEventCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string quotedClickTargetName =
      base::GetQuotedJSONString(event->click_target->name.c_str());
  std::string script;
  switch (event->type) {
    case mojom::FileSelectorEventType::CLICK_OK:
      script = kScriptClickOk;
      break;
    case mojom::FileSelectorEventType::CLICK_CANCEL:
      script = kScriptClickCancel;
      break;
    case mojom::FileSelectorEventType::CLICK_DIRECTORY:
      script = base::StringPrintf(kScriptClickDirectory,
                                  quotedClickTargetName.c_str());
      break;
    case mojom::FileSelectorEventType::CLICK_FILE:
      script =
          base::StringPrintf(kScriptClickFile, quotedClickTargetName.c_str());
      break;
  }
  dialog_holder_->ExecuteJavaScript(script, {});

  std::move(callback).Run();
}

void ArcSelectFilesHandler::GetFileSelectorElements(
    mojom::GetFileSelectorElementsRequestPtr request,
    mojom::FileSystemHost::GetFileSelectorElementsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  dialog_holder_->ExecuteJavaScript(
      kScriptGetElements,
      base::BindOnce(&OnGetElementsScriptResults, std::move(callback)));
}

void ArcSelectFilesHandler::SetDialogHolderForTesting(
    std::unique_ptr<SelectFileDialogHolder> dialog_holder) {
  dialog_holder_ = std::move(dialog_holder);
}

SelectFileDialogHolder::SelectFileDialogHolder(
    ui::SelectFileDialog::Listener* listener) {
  select_file_dialog_ = static_cast<SelectFileDialogExtension*>(
      ui::SelectFileDialog::Create(listener, nullptr).get());
}

SelectFileDialogHolder::~SelectFileDialogHolder() {
  // select_file_dialog_ can be nullptr only in unit tests.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

bool SelectFileDialogHolder::SelectFile(
    ui::SelectFileDialog::Type type,
    const base::FilePath& default_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    int task_id,
    bool show_android_picker_apps) {
  aura::Window* owner_window = nullptr;
  for (auto* window : ChromeLauncherController::instance()->GetArcWindows()) {
    if (arc::GetWindowTaskId(window) == task_id) {
      owner_window = window;
      break;
    }
  }
  if (!owner_window) {
    LOG(ERROR) << "Can't find the ARC window for task ID : " << task_id;
    return false;
  }

  select_file_dialog_->SelectFileWithFileManagerParams(
      type,
      /*title=*/base::string16(), default_path, file_types,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(), owner_window,
      /*params=*/nullptr, task_id, show_android_picker_apps);
  return true;
}

void SelectFileDialogHolder::ExecuteJavaScript(
    const std::string& script,
    content::RenderFrameHost::JavaScriptResultCallback callback) {
  content::RenderViewHost* view_host = select_file_dialog_->GetRenderViewHost();
  content::RenderFrameHost* frame_host =
      view_host ? view_host->GetMainFrame() : nullptr;

  if (!frame_host) {
    LOG(ERROR) << "Can't execute a script. SelectFileDialog is not ready.";
    if (callback)
      std::move(callback).Run(base::Value());
    return;
  }

  frame_host->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(script), std::move(callback),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

}  // namespace arc
