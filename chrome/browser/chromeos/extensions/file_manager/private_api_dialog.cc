// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_dialog.h"

#include <stddef.h>

#include "base/bind.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_select_files_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/select_file_dialog_extension_user_data.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

using content::BrowserThread;

namespace extensions {

namespace {

// Computes the routing ID for SelectFileDialogExtension from the |function|.
SelectFileDialogExtension::RoutingID GetFileDialogRoutingID(
    ExtensionFunction* function) {
  return SelectFileDialogExtensionUserData::GetRoutingIdForWebContents(
      function->GetSenderWebContents());
}

}  // namespace

ExtensionFunction::ResponseAction
FileManagerPrivateCancelDialogFunction::Run() {
  SelectFileDialogExtension::OnFileSelectionCanceled(
      GetFileDialogRoutingID(this));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateSelectFileFunction::Run() {
  using extensions::api::file_manager_private::SelectFile::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<GURL> file_paths;
  file_paths.emplace_back(params->selected_path);

  file_manager::util::GetSelectedFileInfoLocalPathOption option =
      file_manager::util::NO_LOCAL_PATH_RESOLUTION;
  if (params->should_return_local_path) {
    option = params->for_opening ?
        file_manager::util::NEED_LOCAL_PATH_FOR_OPENING :
        file_manager::util::NEED_LOCAL_PATH_FOR_SAVING;
  }

  ChromeExtensionFunctionDetails chrome_details(this);
  file_manager::util::GetSelectedFileInfo(
      render_frame_host(), chrome_details.GetProfile(), file_paths, option,
      base::BindOnce(
          &FileManagerPrivateSelectFileFunction::GetSelectedFileInfoResponse,
          this, params->for_opening, params->index));
  return RespondLater();
}

void FileManagerPrivateSelectFileFunction::GetSelectedFileInfoResponse(
    bool for_open,
    int index,
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (files.size() != 1) {
    Respond(Error("No file selected"));
    return;
  }
  SelectFileDialogExtension::OnFileSelected(GetFileDialogRoutingID(this),
                                            files[0], index);
  ChromeExtensionFunctionDetails chrome_details(this);
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              chrome_details.GetProfile())) {
    notifier->NotifyFileDialogSelection({files[0]}, for_open);
  }
  Respond(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateSelectFilesFunction::Run() {
  using extensions::api::file_manager_private::SelectFiles::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<GURL> file_urls;
  for (size_t i = 0; i < params->selected_paths.size(); ++i)
    file_urls.emplace_back(params->selected_paths[i]);

  ChromeExtensionFunctionDetails chrome_details(this);
  file_manager::util::GetSelectedFileInfo(
      render_frame_host(), chrome_details.GetProfile(), file_urls,
      params->should_return_local_path
          ? file_manager::util::NEED_LOCAL_PATH_FOR_OPENING
          : file_manager::util::NO_LOCAL_PATH_RESOLUTION,
      base::BindOnce(
          &FileManagerPrivateSelectFilesFunction::GetSelectedFileInfoResponse,
          this, true));
  return RespondLater();
}

void FileManagerPrivateSelectFilesFunction::GetSelectedFileInfoResponse(
    bool for_open,
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (files.empty()) {
    Respond(Error("No files selected"));
    return;
  }

  SelectFileDialogExtension::OnMultiFilesSelected(GetFileDialogRoutingID(this),
                                                  files);
  ChromeExtensionFunctionDetails chrome_details(this);
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              chrome_details.GetProfile())) {
    notifier->NotifyFileDialogSelection(files, for_open);
  }
  Respond(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetAndroidPickerAppsFunction::Run() {
  using extensions::api::file_manager_private::GetAndroidPickerApps::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* intent_helper = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->intent_helper(),
      RequestIntentHandlerList);
  if (!intent_helper)
    return RespondNow(Error("Can't get ARC intent helper"));

  arc::mojom::IntentInfoPtr intent = arc::mojom::IntentInfo::New();
  intent->action = "android.intent.action.GET_CONTENT";
  std::vector<std::string> categories;
  categories.push_back("android.intent.category.OPENABLE");
  intent->categories = categories;

  // Convert extensions to MIME
  std::vector<std::string> mime_types;
  for (const std::string& extension : params->extensions) {
    std::string mime_type;
    if (net::GetMimeTypeFromExtension(extension, &mime_type)) {
      mime_types.push_back(mime_type);
    } else {
      LOG(ERROR) << "Failed to get MIME type for: " << extension;
    }
  }
  if (mime_types.size() == 1) {
    intent->type = mime_types[0];
  } else {
    intent->type = "*/*";
    intent->extras = base::flat_map<std::string, std::string>();
    for (const std::string& mime_type : mime_types) {
      intent->extras.value().insert(
          std::make_pair("android.intent.extra.MIME_TYPES", mime_type));
    }
  }

  intent_helper->RequestIntentHandlerList(
      std::move(intent),
      base::BindOnce(
          &FileManagerPrivateGetAndroidPickerAppsFunction::OnActivitiesLoaded,
          this));
  return RespondLater();
}

void FileManagerPrivateGetAndroidPickerAppsFunction::OnActivitiesLoaded(
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  auto* intent_helper =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile);
  std::vector<arc::ArcIntentHelperBridge::ActivityName> activity_names;
  for (const auto& handler : handlers) {
    activity_names.emplace_back(handler->package_name, handler->activity_name);
  }
  intent_helper->GetActivityIcons(
      activity_names,
      base::BindOnce(
          &FileManagerPrivateGetAndroidPickerAppsFunction::OnIconsLoaded, this,
          std::move(handlers)));
}

void FileManagerPrivateGetAndroidPickerAppsFunction::OnIconsLoaded(
    std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using api::file_manager_private::FileTask;
  std::vector<api::file_manager_private::AndroidApp> results;
  for (const auto& handler : handlers) {
    if (arc::IsPickerPackageToExclude(handler->package_name))
      continue;

    api::file_manager_private::AndroidApp app;
    app.name = handler->name;
    app.package_name = handler->package_name;
    app.activity_name = handler->activity_name;
    auto it = icons->find(arc::ArcIntentHelperBridge::ActivityName(
        handler->package_name, handler->activity_name));
    if (it != icons->end()) {
      app.icon_set = std::make_unique<api::file_manager_private::IconSet>();
      app.icon_set->icon32x32_url =
          std::make_unique<std::string>(it->second.icon16_dataurl->data.spec());
    }
    results.push_back(std::move(app));
  }
  Respond(ArgumentList(extensions::api::file_manager_private::
                           GetAndroidPickerApps::Results::Create(results)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateSelectAndroidPickerAppFunction::Run() {
  using extensions::api::file_manager_private::SelectAndroidPickerApp::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Though the user didn't select an actual file, we generate a virtual file
  // path that represents the selected picker app and pass it back to the dialog
  // listener with OnFileSelected function.
  ui::SelectedFileInfo file;
  file.file_path = arc::ConvertAndroidActivityToFilePath(
      params->android_app.package_name, params->android_app.activity_name);
  SelectFileDialogExtension::OnFileSelected(GetFileDialogRoutingID(this), file,
                                            0);
  return RespondNow(NoArguments());
}

}  // namespace extensions
