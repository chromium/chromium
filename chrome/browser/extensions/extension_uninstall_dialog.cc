// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_app_icon_service.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_url_handlers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/native_window_tracker.h"
#include "url/origin.h"

namespace extensions {

namespace {

constexpr int kIconSize = 64;

constexpr char16_t kExtensionRemovedError[] =
    u"Extension was removed before dialog closed.";

constexpr char kReferrerId[] = "chrome-remove-extension-dialog";

float GetScaleFactor(gfx::NativeWindow window) {
  const display::Screen* screen = display::Screen::GetScreen();
  if (!screen)
    return 1.0;  // Happens in unit_tests.
  if (window)
    return screen->GetDisplayNearestWindow(window).device_scale_factor();
  return screen->GetPrimaryDisplay().device_scale_factor();
}

ExtensionUninstallDialog::OnWillShowCallback* g_on_will_show_callback = nullptr;
}  // namespace

void ExtensionUninstallDialog::SetOnShownCallbackForTesting(
    ExtensionUninstallDialog::OnWillShowCallback* callback) {
  g_on_will_show_callback = callback;
}

ExtensionUninstallDialog::ExtensionUninstallDialog(
    Profile* profile,
    gfx::NativeWindow parent,
    ExtensionUninstallDialog::Delegate* delegate)
    : profile_(profile), parent_(parent), delegate_(delegate) {
  DCHECK(delegate_);
  if (parent)
    parent_window_tracker_ = views::NativeWindowTracker::Create(parent);
  profile_observation_.Observe(profile_.get());
}

ExtensionUninstallDialog::~ExtensionUninstallDialog() = default;

void ExtensionUninstallDialog::ConfirmUninstallByExtension(
    const scoped_refptr<const Extension>& extension,
    const scoped_refptr<const Extension>& triggering_extension,
    UninstallReason reason,
    UninstallSource source) {
  triggering_extension_ = triggering_extension;
  ConfirmUninstall(extension, reason, source);
}

void ExtensionUninstallDialog::ConfirmUninstall(
    const scoped_refptr<const Extension>& extension,
    UninstallReason reason,
    UninstallSource source) {
  DCHECK(thread_checker_.CalledOnValidThread());

  UMA_HISTOGRAM_ENUMERATION("Extensions.UninstallSource", source,
                            NUM_UNINSTALL_SOURCES);

  extension_ = extension;
  uninstall_reason_ = reason;

  if (!profile_)
    return;

  if (parent() && parent_window_tracker_->WasNativeWindowDestroyed()) {
    OnDialogClosed(CLOSE_ACTION_CANCELED);
    return;
  }

  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile_);
  show_report_abuse_checkbox_ =
      extension_management->UpdatesFromWebstore(*extension_);

  // Track that extension uninstalled externally.
  registry_observation_.Observe(ExtensionRegistry::Get(profile_));

  // Dialog will be shown once icon is loaded.
  DCHECK(!dialog_shown_);
  icon_ = ChromeAppIconService::Get(profile_)->CreateIcon(this, extension->id(),
                                                          kIconSize);
  icon_->image_skia().GetRepresentation(GetScaleFactor(parent_));
}

void ExtensionUninstallDialog::OnIconUpdated(ChromeAppIcon* icon) {
  // Ignore initial update.
  if (!icon_ || dialog_shown_)
    return;
  DCHECK_EQ(icon, icon_.get());

  dialog_shown_ = true;

  if (parent() && parent_window_tracker_->WasNativeWindowDestroyed()) {
    OnDialogClosed(CLOSE_ACTION_CANCELED);
    return;
  }

  if (g_on_will_show_callback != nullptr)
    g_on_will_show_callback->Run(this);

  switch (ScopedTestDialogAutoConfirm::GetAutoConfirmValue()) {
    case ScopedTestDialogAutoConfirm::NONE:
      Show();
      break;
    case ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION:
    case ScopedTestDialogAutoConfirm::ACCEPT_AND_REMEMBER_OPTION:
      OnDialogClosed(CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED);
      break;
    case ScopedTestDialogAutoConfirm::ACCEPT:
      OnDialogClosed(CLOSE_ACTION_UNINSTALL);
      break;
    case ScopedTestDialogAutoConfirm::CANCEL:
      OnDialogClosed(CLOSE_ACTION_CANCELED);
      break;
  }
}

void ExtensionUninstallDialog::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  // Handle the case when extension was uninstalled externally and we have to
  // close current dialog.
  if (extension != extension_)
    return;

  extension_uninstalled_early_ = true;
  Close();
}

void ExtensionUninstallDialog::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile_, profile);
  profile_ = nullptr;
  profile_observation_.Reset();
  OnDialogClosed(CLOSE_ACTION_CANCELED);
}

std::string ExtensionUninstallDialog::GetHeadingText() {
  if (triggering_extension_) {
    return l10n_util::GetStringFUTF8(
        IDS_EXTENSION_PROGRAMMATIC_UNINSTALL_PROMPT_HEADING,
        base::UTF8ToUTF16(triggering_extension_->name()),
        base::UTF8ToUTF16(extension_->name()));
  }
  return l10n_util::GetStringFUTF8(IDS_EXTENSION_UNINSTALL_PROMPT_HEADING,
                                   base::UTF8ToUTF16(extension_->name()));
}

GURL ExtensionUninstallDialog::GetLaunchURL() const {
  return AppLaunchInfo::GetFullLaunchURL(extension_.get());
}

bool ExtensionUninstallDialog::ShouldShowCheckbox() const {
  return show_report_abuse_checkbox_;
}

std::u16string ExtensionUninstallDialog::GetCheckboxLabel() const {
  DCHECK(ShouldShowCheckbox());

  return triggering_extension_.get()
             ? l10n_util::GetStringFUTF16(
                   IDS_EXTENSION_PROMPT_UNINSTALL_REPORT_ABUSE_FROM_EXTENSION,
                   base::UTF8ToUTF16(extension_->name()))
             : l10n_util::GetStringUTF16(
                   IDS_EXTENSION_PROMPT_UNINSTALL_REPORT_ABUSE);
}

void ExtensionUninstallDialog::OnDialogClosed(CloseAction action) {
  // Ensure the dialog isn't notified of an uninstallation after the dialog was
  // closed.
  registry_observation_.Reset();

  bool success = false;
  std::u16string error;
  switch (action) {
    case CLOSE_ACTION_UNINSTALL_AND_CHECKBOX_CHECKED:
      DCHECK(profile_);
      success = Uninstall(&error);
      base::RecordAction(base::UserMetricsAction(
          "Extensions.UninstallDialogReportAbuseChecked"));
      base::RecordAction(
          base::UserMetricsAction("Extensions.UninstallDialogRemoveClick"));
      // If the extension specifies a custom uninstall page via
      // chrome.runtime.setUninstallURL, then at uninstallation its uninstall
      // page opens. To ensure that the CWS Report Abuse page is the active
      // tab at uninstallation, HandleReportAbuse() is called after
      // Uninstall().
      HandleReportAbuse();
      break;
    case CLOSE_ACTION_UNINSTALL:
      base::RecordAction(
          base::UserMetricsAction("Extensions.UninstallDialogRemoveClick"));
      success = Uninstall(&error);
      break;
    case CLOSE_ACTION_CANCELED:
      base::RecordAction(
          base::UserMetricsAction("Extensions.UninstallDialogCancelClick"));
      error = extension_uninstalled_early_ ? kExtensionRemovedError
                                           : u"User canceled uninstall dialog";
      break;
    case CLOSE_ACTION_LAST:
      NOTREACHED();
  }
  delegate_->OnExtensionUninstallDialogClosed(success, error);
}

bool ExtensionUninstallDialog::Uninstall(std::u16string* error) {
  DCHECK(profile_);
  const Extension* current_extension =
      ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_->id(), ExtensionRegistry::EVERYTHING);
  if (current_extension) {
    if (current_extension->was_installed_by_default()) {
      base::RecordAction(base::UserMetricsAction(
          "Extensions.RemovedDefaultInstalledExtension"));
    }

    return ExtensionSystem::Get(profile_)
        ->extension_service()
        ->UninstallExtension(extension_->id(), uninstall_reason_, error);
  }
  *error = kExtensionRemovedError;
  return false;
}

void ExtensionUninstallDialog::HandleReportAbuse() {
  DCHECK(profile_);
  NavigateParams params(
      profile_,
      extension_urls::GetWebstoreReportAbuseUrl(extension_->id(), kReferrerId),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

}  // namespace extensions
