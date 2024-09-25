// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_select_files_handler.h"

#include <utility>

#include "ash/components/arc/arc_util.h"
#include "base/functional/bind.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_select_files_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace arc {
namespace {

constexpr char kRecentAllFakePath[] = "/.fake-entry/recent/all";

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
    ConvertToElementVector(value.GetDict().Find("dirNames"),
                           &result->directory_elements);
    ConvertToElementVector(value.GetDict().Find("fileNames"),
                           &result->file_elements);
    // TODO(niwa): Fill result->search_query.
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
    case mojom::SelectFilesActionType::OPEN_MEDIA_STORE_FILES:
      return request->allow_multiple
                 ? ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE
                 : ui::SelectFileDialog::SELECT_OPEN_FILE;
    case mojom::SelectFilesActionType::OPEN_DOCUMENT_TREE:
      return ui::SelectFileDialog::SELECT_EXISTING_FOLDER;
    case mojom::SelectFilesActionType::CREATE_DOCUMENT:
      return ui::SelectFileDialog::SELECT_SAVEAS_FILE;
  }
  NOTREACHED_IN_MIGRATION();
}

base::FilePath GetInitialFilePath(const mojom::SelectFilesRequestPtr& request) {
  const mojom::DocumentPathPtr& document_path = request->initial_document_path;
  if (!document_path)
    return base::FilePath(kRecentAllFakePath);

  if (!document_path->root_id.has_value()) {
    LOG(ERROR) << "root ID is missing; falling back to opening Recent";
    return base::FilePath(kRecentAllFakePath);
  }

  // TODO(niwa): Convert non-root document IDs to the relative path and append.
  return arc::GetDocumentsProviderMountPath(document_path->authority,
                                            document_path->root_id.value());
}

void BuildFileTypeInfo(const mojom::SelectFilesRequestPtr& request,
                       ui::SelectFileDialog::FileTypeInfo* file_type_info) {
  file_type_info->allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
  for (const std::string& mime_type : request->mime_types) {
    const std::vector<base::FilePath::StringType> extensions =
        GetExtensionsForArcMimeType(mime_type);
    if (!extensions.empty()) {
      file_type_info->extensions.push_back(extensions);
    }

    // Enable "Select from all files" option if GetExtensionsForArcMimeType
    // can't find any matching extensions or specified MIME type contains an
    // asterisk. This is to support extensions that are not covered by
    // GetExtensionsForArcMimeType. (crbug.com/1034874)
    if (extensions.empty() ||
        base::EndsWith(mime_type, "/*", base::CompareCase::SENSITIVE)) {
      file_type_info->include_all_files = true;
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
  std::string search_query = request->search_query.value_or(std::string());

  // Android picker apps should be shown in GET_CONTENT mode.
  bool show_android_picker_apps =
      request->action_type == mojom::SelectFilesActionType::GET_CONTENT;

  // In OPEN_MEDIA_STORE_FILES mode, only show volumes indexed in Android's
  // MediaStore.
  bool use_media_store_filter =
      request->action_type ==
      mojom::SelectFilesActionType::OPEN_MEDIA_STORE_FILES;

  bool success = dialog_holder_->SelectFile(
      dialog_type, default_path, &file_type_info, request->task_id,
      search_query, show_android_picker_apps, use_media_store_filter);
  if (!success) {
    std::move(callback_).Run(mojom::SelectFilesResult::New());
  }
}

void ArcSelectFilesHandler::FileSelected(const ui::SelectedFileInfo& file,
                                         int index) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);

  const std::string& activity = ConvertFilePathToAndroidActivity(file.path());
  if (!activity.empty()) {
    // The user selected an Android picker activity instead of a file.
    mojom::SelectFilesResultPtr result = mojom::SelectFilesResult::New();
    result->picker_activity = activity;
    std::move(callback_).Run(std::move(result));
    return;
  }

  FilesSelectedInternal({file});
}

void ArcSelectFilesHandler::MultiFilesSelected(
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FilesSelectedInternal(files);
}

void ArcSelectFilesHandler::FileSelectionCanceled() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);
  // Returns an empty result if the user cancels file selection.
  std::move(callback_).Run(mojom::SelectFilesResult::New());
}

void ArcSelectFilesHandler::FilesSelectedInternal(
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK(callback_);

  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile_);

  std::vector<storage::FileSystemURL> file_system_urls;
  for (const auto& file : files) {
    GURL gurl;
    file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
        profile_, file.path(), file_manager::util::GetFileManagerURL(), &gurl);
    file_system_urls.push_back(
        file_system_context->CrackURLInFirstPartyContext(gurl));
  }

  arc::ConvertToContentUrlsAndShare(
      ProfileManager::GetPrimaryUserProfile(), file_system_urls,
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
    const std::string& search_query,
    bool show_android_picker_apps,
    bool use_media_store_filter) {
  aura::Window* owner_window = nullptr;
  for (auto* window : ChromeShelfController::instance()->GetArcWindows()) {
    if (arc::GetWindowTaskId(window) == task_id) {
      owner_window = window;
      break;
    }
  }
  if (!owner_window) {
    LOG(ERROR) << "Can't find the ARC window for task ID : " << task_id;
    return false;
  }

  SelectFileDialogExtension::Owner owner;
  owner.window = owner_window;
  owner.android_task_id = task_id;
  owner.dialog_caller =
      policy::DlpFileDestination(data_controls::Component::kArc);
  select_file_dialog_->SelectFileWithFileManagerParams(
      type,
      /*title=*/std::u16string(), default_path, file_types,
      /*file_type_index=*/0, owner, search_query, show_android_picker_apps,
      use_media_store_filter);
  return true;
}

void SelectFileDialogHolder::ExecuteJavaScript(
    const std::string& script,
    content::RenderFrameHost::JavaScriptResultCallback callback) {
  content::RenderFrameHost* frame_host =
      select_file_dialog_->GetPrimaryMainFrame();

  if (!frame_host || !frame_host->IsRenderFrameLive()) {
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
