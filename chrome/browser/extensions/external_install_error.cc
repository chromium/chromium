// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_install_error.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_install_error_menu_item_id_provider.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_install_error_constants.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace extensions {

namespace {

// Return the menu label for a global error.
base::string16 GetMenuItemLabel(const Extension* extension) {
  if (!extension)
    return base::string16();

  int id = -1;
  if (extension->is_app())
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_APP;
  else if (extension->is_theme())
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_THEME;
  else
    id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_EXTENSION;

  return l10n_util::GetStringFUTF16(id, base::UTF8ToUTF16(extension->name()));
}

ExternalInstallError::DefaultDialogButtonSetting
MapDefaultButtonStringToSetting(const std::string& button_setting_string) {
  if (button_setting_string == kDefaultDialogButtonSettingOk)
    return ExternalInstallError::DIALOG_BUTTON_OK;
  if (button_setting_string == kDefaultDialogButtonSettingCancel)
    return ExternalInstallError::DIALOG_BUTTON_CANCEL;
  if (button_setting_string == kDefaultDialogButtonSettingNoDefault)
    return ExternalInstallError::NO_DEFAULT_DIALOG_BUTTON;

  NOTREACHED() << "Unexpected default button string: " << button_setting_string;
  return ExternalInstallError::NOT_SPECIFIED;
}

// A global error that spawns a dialog when the menu item is clicked.
class ExternalInstallMenuAlert : public GlobalError {
 public:
  explicit ExternalInstallMenuAlert(ExternalInstallError* error);
  ~ExternalInstallMenuAlert() override;

 private:
  // GlobalError implementation.
  Severity GetSeverity() override;
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  base::string16 MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;
  bool HasBubbleView() override;
  bool HasShownBubbleView() override;
  void ShowBubbleView(Browser* browser) override;
  GlobalErrorBubbleViewBase* GetBubbleView() override;

  // The owning ExternalInstallError.
  ExternalInstallError* error_;

  // Provides menu item id for GlobalError.
  ExtensionInstallErrorMenuItemIdProvider id_provider_;

  DISALLOW_COPY_AND_ASSIGN(ExternalInstallMenuAlert);
};

// A global error that spawns a bubble when the menu item is clicked.
class ExternalInstallBubbleAlert : public GlobalErrorWithStandardBubble {
 public:
  ExternalInstallBubbleAlert(ExternalInstallError* error,
                             ExtensionInstallPrompt::Prompt* prompt);
  ~ExternalInstallBubbleAlert() override;

 private:
  // GlobalError implementation.
  Severity GetSeverity() override;
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  base::string16 MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;

  // GlobalErrorWithStandardBubble implementation.
  gfx::Image GetBubbleViewIcon() override;
  base::string16 GetBubbleViewTitle() override;
  std::vector<base::string16> GetBubbleViewMessages() override;
  base::string16 GetBubbleViewAcceptButtonLabel() override;
  base::string16 GetBubbleViewCancelButtonLabel() override;
  int GetDefaultDialogButton() const override;
  void OnBubbleViewDidClose(Browser* browser) override;
  void BubbleViewAcceptButtonPressed(Browser* browser) override;
  void BubbleViewCancelButtonPressed(Browser* browser) override;

  // The owning ExternalInstallError.
  ExternalInstallError* error_;
  ExtensionInstallErrorMenuItemIdProvider id_provider_;

  // The Prompt with all information, which we then use to populate the bubble.
  // Owned by |error|.
  ExtensionInstallPrompt::Prompt* prompt_;

  DISALLOW_COPY_AND_ASSIGN(ExternalInstallBubbleAlert);
};

////////////////////////////////////////////////////////////////////////////////
// ExternalInstallMenuAlert

ExternalInstallMenuAlert::ExternalInstallMenuAlert(ExternalInstallError* error)
    : error_(error) {
}

ExternalInstallMenuAlert::~ExternalInstallMenuAlert() {
}

GlobalError::Severity ExternalInstallMenuAlert::GetSeverity() {
  return SEVERITY_LOW;
}

bool ExternalInstallMenuAlert::HasMenuItem() {
  return true;
}

int ExternalInstallMenuAlert::MenuItemCommandID() {
  return id_provider_.menu_command_id();
}

base::string16 ExternalInstallMenuAlert::MenuItemLabel() {
  return GetMenuItemLabel(error_->GetExtension());
}

void ExternalInstallMenuAlert::ExecuteMenuItem(Browser* browser) {
  error_->ShowDialog(browser);
}

bool ExternalInstallMenuAlert::HasBubbleView() {
  return false;
}

bool ExternalInstallMenuAlert::HasShownBubbleView() {
  NOTREACHED();
  return true;
}

void ExternalInstallMenuAlert::ShowBubbleView(Browser* browser) {
  NOTREACHED();
}

GlobalErrorBubbleViewBase* ExternalInstallMenuAlert::GetBubbleView() {
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ExternalInstallBubbleAlert

ExternalInstallBubbleAlert::ExternalInstallBubbleAlert(
    ExternalInstallError* error,
    ExtensionInstallPrompt::Prompt* prompt)
    : error_(error), prompt_(prompt) {
  DCHECK(error_);
  DCHECK(prompt_);
}

ExternalInstallBubbleAlert::~ExternalInstallBubbleAlert() {
}

GlobalError::Severity ExternalInstallBubbleAlert::GetSeverity() {
  return SEVERITY_LOW;
}

bool ExternalInstallBubbleAlert::HasMenuItem() {
  return true;
}

int ExternalInstallBubbleAlert::MenuItemCommandID() {
  return id_provider_.menu_command_id();
}

base::string16 ExternalInstallBubbleAlert::MenuItemLabel() {
  return GetMenuItemLabel(error_->GetExtension());
}

void ExternalInstallBubbleAlert::ExecuteMenuItem(Browser* browser) {
  // |browser| is nullptr in unit test.
  if (browser)
    ShowBubbleView(browser);
  error_->DidOpenBubbleView();
}

gfx::Image ExternalInstallBubbleAlert::GetBubbleViewIcon() {
  if (prompt_->icon().IsEmpty())
    return GlobalErrorWithStandardBubble::GetBubbleViewIcon();
  // Scale icon to a reasonable size.
  return gfx::Image(gfx::ImageSkiaOperations::CreateResizedImage(
      *prompt_->icon().ToImageSkia(),
      skia::ImageOperations::RESIZE_BEST,
      gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                extension_misc::EXTENSION_ICON_SMALL)));
}

base::string16 ExternalInstallBubbleAlert::GetBubbleViewTitle() {
  return l10n_util::GetStringFUTF16(
      IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_BUBBLE_TITLE,
      base::UTF8ToUTF16(prompt_->extension()->name()));
}

std::vector<base::string16>
ExternalInstallBubbleAlert::GetBubbleViewMessages() {
  std::vector<base::string16> messages;
  int heading_id =
      IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_BUBBLE_HEADING_EXTENSION;
  if (prompt_->extension()->is_app())
    heading_id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_BUBBLE_HEADING_APP;
  else if (prompt_->extension()->is_theme())
    heading_id = IDS_EXTENSION_EXTERNAL_INSTALL_ALERT_BUBBLE_HEADING_THEME;
  messages.push_back(l10n_util::GetStringUTF16(heading_id));

  if (prompt_->GetPermissionCount()) {
    messages.push_back(prompt_->GetPermissionsHeading());
    for (size_t i = 0; i < prompt_->GetPermissionCount(); ++i) {
      messages.push_back(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PERMISSION_LINE, prompt_->GetPermission(i)));
    }
  }
  // TODO(yoz): OAuth issue advice?
  return messages;
}

int ExternalInstallBubbleAlert::GetDefaultDialogButton() const {
  switch (error_->default_dialog_button_setting()) {
    case ExternalInstallError::DIALOG_BUTTON_OK:
      return ui::DIALOG_BUTTON_OK;
    case ExternalInstallError::DIALOG_BUTTON_CANCEL:
      return ui::DIALOG_BUTTON_CANCEL;
    case ExternalInstallError::NO_DEFAULT_DIALOG_BUTTON:
      return ui::DIALOG_BUTTON_NONE;
    case ExternalInstallError::NOT_SPECIFIED:
      break;
  }
  return GlobalErrorWithStandardBubble::GetDefaultDialogButton();
}

base::string16 ExternalInstallBubbleAlert::GetBubbleViewAcceptButtonLabel() {
  return prompt_->GetAcceptButtonLabel();
}

base::string16 ExternalInstallBubbleAlert::GetBubbleViewCancelButtonLabel() {
  return prompt_->GetAbortButtonLabel();
}

void ExternalInstallBubbleAlert::OnBubbleViewDidClose(Browser* browser) {
  error_->DidCloseBubbleView();
}

void ExternalInstallBubbleAlert::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  error_->OnInstallPromptDone(ExtensionInstallPrompt::Result::ACCEPTED);
}

void ExternalInstallBubbleAlert::BubbleViewCancelButtonPressed(
    Browser* browser) {
  error_->OnInstallPromptDone(ExtensionInstallPrompt::Result::USER_CANCELED);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExternalInstallError

// static
ExternalInstallError::DefaultDialogButtonSetting
ExternalInstallError::GetDefaultDialogButton(
    const base::Value& webstore_response) {
  const base::Value* value = webstore_response.FindKeyOfType(
      kExternalInstallDefaultButtonKey, base::Value::Type::STRING);
  if (value) {
    return MapDefaultButtonStringToSetting(value->GetString());
  }

  if (base::FeatureList::IsEnabled(
          ::features::kExternalExtensionDefaultButtonControl)) {
    std::string default_button = base::GetFieldTrialParamValueByFeature(
        ::features::kExternalExtensionDefaultButtonControl,
        kExternalInstallDefaultButtonKey);
    if (!default_button.empty()) {
      return MapDefaultButtonStringToSetting(default_button);
    }
  }

  return NOT_SPECIFIED;
}

ExternalInstallError::ExternalInstallError(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    AlertType alert_type,
    ExternalInstallManager* manager)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      alert_type_(alert_type),
      manager_(manager),
      error_service_(GlobalErrorServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_))) {
  prompt_.reset(new ExtensionInstallPrompt::Prompt(
      ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT));

  webstore_data_fetcher_.reset(
      new WebstoreDataFetcher(this, GURL(), extension_id_));
  webstore_data_fetcher_->Start(
      content::BrowserContext::GetDefaultStoragePartition(browser_context_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}

ExternalInstallError::~ExternalInstallError() {
#if DCHECK_IS_ON()
  // Errors should only be removed while the profile is valid, since removing
  // the error can trigger other subsystems listening for changes.
  BrowserContextDependencyManager::GetInstance()
      ->AssertBrowserContextWasntDestroyed(browser_context_);
#endif
  if (global_error_.get())
    error_service_->RemoveUnownedGlobalError(global_error_.get());
}

void ExternalInstallError::OnInstallPromptDone(
    ExtensionInstallPrompt::Result result) {
  const Extension* extension = GetExtension();

  // If the error isn't removed and deleted as part of handling the user's
  // response (which can happen, e.g., if an uninstall fails), be sure to remove
  // the error directly in order to ensure it's not called twice.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ExternalInstallError::RemoveError,
                                weak_factory_.GetWeakPtr()));

  switch (result) {
    case ExtensionInstallPrompt::Result::ACCEPTED:
      if (extension) {
        ExtensionSystem::Get(browser_context_)
            ->extension_service()
            ->GrantPermissionsAndEnableExtension(extension);
      }
      break;
    case ExtensionInstallPrompt::Result::USER_CANCELED:
      if (extension) {
        ExtensionSystem::Get(browser_context_)
            ->extension_service()
            ->UninstallExtension(extension_id_,
                                 extensions::UNINSTALL_REASON_INSTALL_CANCELED,
                                 nullptr);  // Ignore error.
      }
      break;
    case ExtensionInstallPrompt::Result::ABORTED:
      manager_->DidChangeInstallAlertVisibility(this, false);
      break;
  }
  // NOTE: We may be deleted here!
}

void ExternalInstallError::DidOpenBubbleView() {
  manager_->DidChangeInstallAlertVisibility(this, true);
}

void ExternalInstallError::DidCloseBubbleView() {
  manager_->DidChangeInstallAlertVisibility(this, false);
}

void ExternalInstallError::ShowDialog(Browser* browser) {
  DCHECK(install_ui_.get());
  DCHECK(prompt_.get());
  DCHECK(browser);
  content::WebContents* web_contents = NULL;
  web_contents = browser->tab_strip_model()->GetActiveWebContents();
  install_ui_show_params_.reset(
      new ExtensionInstallPromptShowParams(web_contents));
  manager_->DidChangeInstallAlertVisibility(this, true);
  ExtensionInstallPrompt::GetDefaultShowDialogCallback().Run(
      install_ui_show_params_.get(),
      base::Bind(&ExternalInstallError::OnInstallPromptDone,
                 weak_factory_.GetWeakPtr()),
      std::move(prompt_));
}

const Extension* ExternalInstallError::GetExtension() const {
  return ExtensionRegistry::Get(browser_context_)
      ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
}

void ExternalInstallError::OnWebstoreRequestFailure() {
  OnFetchComplete();
}

void ExternalInstallError::OnWebstoreResponseParseSuccess(
    std::unique_ptr<base::DictionaryValue> webstore_data) {
  std::string localized_user_count;
  double average_rating = 0;
  int rating_count = 0;
  if (!webstore_data->GetString(kUsersKey, &localized_user_count) ||
      !webstore_data->GetDouble(kAverageRatingKey, &average_rating) ||
      !webstore_data->GetInteger(kRatingCountKey, &rating_count)) {
    // If we don't get a valid webstore response, short circuit, and continue
    // to show a prompt without webstore data.
    OnFetchComplete();
    return;
  }

  default_dialog_button_setting_ = GetDefaultDialogButton(*webstore_data.get());

  bool show_user_count = true;
  webstore_data->GetBoolean(kShowUserCountKey, &show_user_count);

  prompt_->SetWebstoreData(
      localized_user_count, show_user_count, average_rating, rating_count);
  OnFetchComplete();
}

void ExternalInstallError::OnWebstoreResponseParseFailure(
    const std::string& error) {
  OnFetchComplete();
}

void ExternalInstallError::OnFetchComplete() {
  // Create a new ExtensionInstallPrompt. We pass in NULL for the UI
  // components because we display at a later point, and don't want
  // to pass ones which may be invalidated.
  install_ui_.reset(
      new ExtensionInstallPrompt(Profile::FromBrowserContext(browser_context_),
                                 NULL));  // NULL native window.

  install_ui_->ShowDialog(base::Bind(&ExternalInstallError::OnInstallPromptDone,
                                     weak_factory_.GetWeakPtr()),
                          GetExtension(),
                          nullptr,  // Force a fetch of the icon.
                          std::move(prompt_),
                          base::Bind(&ExternalInstallError::OnDialogReady,
                                     weak_factory_.GetWeakPtr()));
}

void ExternalInstallError::OnDialogReady(
    ExtensionInstallPromptShowParams* show_params,
    const ExtensionInstallPrompt::DoneCallback& callback,
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  prompt_ = std::move(prompt);

  if (alert_type_ == BUBBLE_ALERT) {
    global_error_.reset(new ExternalInstallBubbleAlert(this, prompt_.get()));
    error_service_->AddUnownedGlobalError(global_error_.get());

    if (!manager_->has_currently_visible_install_alert()) {
      // |browser| is nullptr during unit tests, so call
      // DidChangeInstallAlertVisibility() regardless because we depend on this
      // in unit tests.
      manager_->DidChangeInstallAlertVisibility(this, true);
      Browser* browser = chrome::FindTabbedBrowser(
          Profile::FromBrowserContext(browser_context_), true);
      if (browser)
        global_error_->ShowBubbleView(browser);
    }
  } else {
    DCHECK(alert_type_ == MENU_ALERT);
    global_error_.reset(new ExternalInstallMenuAlert(this));
    error_service_->AddUnownedGlobalError(global_error_.get());
  }
}

void ExternalInstallError::RemoveError() {
  manager_->RemoveExternalInstallError(extension_id_);
}

}  // namespace extensions
