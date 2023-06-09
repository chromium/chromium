// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/file_manager/file_manager_ui.h"

#include "ash/shell.h"
#include "ash/webui/file_manager/file_manager_page_handler.h"
#include "ash/webui/file_manager/resource_loader.h"
#include "ash/webui/file_manager/resources/grit/file_manager_swa_resources.h"
#include "ash/webui/file_manager/resources/grit/file_manager_swa_resources_map.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/file_manager/grit/file_manager_gen_resources_map.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#include "ui/file_manager/grit/file_manager_resources_map.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash::file_manager {
namespace {

bool IsKioskSession() {
  auto* session_controller = Shell::Get()->session_controller();
  auto account_id = session_controller->GetActiveAccountId();
  const auto user_type =
      session_controller->GetUserSessionByAccountId(account_id)->user_info.type;

  switch (user_type) {
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_CHILD:
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      return false;
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return true;
    case user_manager::NUM_USER_TYPES:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

FileManagerUIConfig::FileManagerUIConfig(
    SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func)
    : SystemWebAppUIConfig(ash::file_manager::kChromeUIFileManagerHost,
                           SystemWebAppType::FILE_MANAGER,
                           create_controller_func) {}

bool FileManagerUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return SystemWebAppUIConfig::IsWebUIEnabled(browser_context) ||
         IsKioskSession();
}

FileManagerUI::FileManagerUI(content::WebUI* web_ui,
                             std::unique_ptr<FileManagerUIDelegate> delegate)
    : MojoWebDialogUI(web_ui), delegate_(std::move(delegate)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Count the number of active windows. This is done so that we can tell if
  // there are any active Files SWA windows.
  ++instance_count_;
  DCHECK_GT(instance_count_, 0);
  DLOG(WARNING) << "Starting FileManagerUI. Open windows: " << instance_count_;

  // Increment the counter each time a window is opened. This is to give a
  // unique ID to each window.
  ++window_counter_;

  delegate_->ShouldPollDriveHostedPinStates(true);

  CreateAndAddTrustedAppDataSource(web_ui, window_counter_);
  // Add ability to request chrome-untrusted: URLs
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

void FileManagerUI::CreateAndAddTrustedAppDataSource(content::WebUI* web_ui,
                                                     int window_number) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIFileManagerHost);

  // Setup chrome://file-manager main and default page.
  source->AddResourcePath("", IDR_FILE_MANAGER_SWA_MAIN_HTML);

  // Add chrome://file-manager content.
  source->AddResourcePaths(
      base::make_span(kFileManagerSwaResources, kFileManagerSwaResourcesSize));

  AddFilesAppResources(source, kFileManagerResources,
                       kFileManagerResourcesSize);
  AddFilesAppResources(source, kFileManagerGenResources,
                       kFileManagerGenResourcesSize);

  // Load time data: add files app strings and feature flags.
  source->EnableReplaceI18nInJS();
  base::Value::Dict dict = delegate_->GetLoadTimeData();
  dict.Set("WINDOW_NUMBER", window_number);
  source->AddLocalizedStrings(dict);
  source->UseStringsJs();

  // Script security policy.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp "
      "chrome://resources "
      "'self' ;");

  // Metadata Shared Worker security policy.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src 'self' ;");

  // Allow using the chrome-untrusted:// scheme in the host.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      "frame-src chrome-untrusted://file-manager "
      "'self';");

  // TODO(crbug.com/1098685): Trusted Type remaining WebUI.
  source->DisableTrustedTypesCSP();
}

int FileManagerUI::GetNumInstances() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return instance_count_;
}

FileManagerUI::~FileManagerUI() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK_GT(instance_count_, 0);
  --instance_count_;

  DLOG(WARNING) << "Stopping FileManagerUI. Open windows: " << instance_count_;

  if (!instance_count_) {
    delegate_->ProgressPausedTasks();
    delegate_->ShouldPollDriveHostedPinStates(false);
  }
}

void FileManagerUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> pending_receiver) {
  if (page_factory_receiver_.is_bound())
    page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void FileManagerUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void FileManagerUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> pending_page,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<FileManagerPageHandler>(
      this, std::move(pending_page_handler), std::move(pending_page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FileManagerUI)

}  // namespace ash::file_manager
