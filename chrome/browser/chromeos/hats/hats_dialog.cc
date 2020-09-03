// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_dialog.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/hats/hats_finch_helper.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/util/version_loader.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

// Default width/height ratio of screen size.
const int kDefaultWidth = 340;
const int kDefaultHeight = 260;

constexpr char kCrOSHaTSURL[] =
    "https://storage.googleapis.com/chromeos-hats-web-stable/index.html";

// Keyword used to join the separate device info elements into a single string
// to be used as site context.
const char kDeviceInfoStopKeyword[] = "&";
const char kDefaultProfileLocale[] = "en-US";

enum class DeviceInfoKey : unsigned int {
  BROWSER = 0,
  PLATFORM,
  FIRMWARE,
  LOCALE,
};

// Maps the given DeviceInfoKey |key| enum to the corresponding string value
// that can be used as a key when creating a URL parameter.
const std::string KeyEnumToString(DeviceInfoKey key) {
  switch (key) {
    case DeviceInfoKey::BROWSER:
      return "browser";
    case DeviceInfoKey::PLATFORM:
      return "platform";
    case DeviceInfoKey::FIRMWARE:
      return "firmware";
    case DeviceInfoKey::LOCALE:
      return "locale";
    default:
      NOTREACHED();
      return std::string();
  }
}

// Must be run on a blocking thread pool.
// Gathers the browser version info, firmware info and platform info and returns
// them in a single encoded string, the format of which is defined below.
// Currently the format is "<key>=<value>&<key>=<value>&<key>=<value>".
std::string GetFormattedSiteContext(const std::string& user_locale,
                                    base::StringPiece join_keyword) {
  std::vector<std::string> pairs;
  pairs.push_back(KeyEnumToString(DeviceInfoKey::BROWSER) + "=" +
                  version_info::GetVersionNumber());

  pairs.push_back(KeyEnumToString(DeviceInfoKey::PLATFORM) + "=" +
                  version_loader::GetVersion(version_loader::VERSION_FULL));

  pairs.push_back(KeyEnumToString(DeviceInfoKey::FIRMWARE) + "=" +
                  version_loader::GetFirmware());

  pairs.push_back(KeyEnumToString(DeviceInfoKey::LOCALE) + "=" + user_locale);

  return base::JoinString(pairs, join_keyword);
}

}  // namespace

// static
std::unique_ptr<HatsDialog> HatsDialog::CreateAndShow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::string user_locale =
      profile->GetPrefs()->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&user_locale);
  if (!user_locale.length())
    user_locale = kDefaultProfileLocale;

  std::unique_ptr<HatsDialog> hats_dialog(
      new HatsDialog(HatsFinchHelper::GetTriggerID(), profile));

  // Raw pointer is used here since the dialog is owned by the hats
  // notification controller which lives until the end of the user session. The
  // dialog will always be closed before that time instant.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetFormattedSiteContext, user_locale,
                     kDeviceInfoStopKeyword),
      base::BindOnce(&HatsDialog::Show, base::Unretained(hats_dialog.get())));

  return hats_dialog;
}

void HatsDialog::Show(const std::string& site_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Link the trigger ID to fetch the correct survey.
  url_ = std::string(kCrOSHaTSURL) + "?" + site_context +
         "&trigger=" + trigger_id_;

  chrome::ShowWebDialog(nullptr, ProfileManager::GetActiveUserProfile(), this);
}

HatsDialog::HatsDialog(const std::string& trigger_id, Profile* user_profile)
    : trigger_id_(trigger_id), user_profile_(user_profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  set_can_resize(false);
}

HatsDialog::~HatsDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(user_profile_);
}

ui::ModalType HatsDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 HatsDialog::GetDialogTitle() const {
  return base::string16();
}

GURL HatsDialog::GetDialogContentURL() const {
  return GURL(url_);
}

void HatsDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {}

void HatsDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string HatsDialog::GetDialogArgs() const {
  return std::string();
}

void HatsDialog::OnDialogClosed(const std::string& json_retval) {}

void HatsDialog::OnCloseContents(WebContents* source, bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool HatsDialog::ShouldShowDialogTitle() const {
  return false;
}

bool HatsDialog::ShouldShowCloseButton() const {
  return false;
}

bool HatsDialog::HandleContextMenu(content::RenderFrameHost* render_frame_host,
                                   const content::ContextMenuParams& params) {
  // Disable context menu
  return true;
}

ui::WebDialogDelegate::FrameKind HatsDialog::GetWebDialogFrameKind() const {
  return ui::WebDialogDelegate::FrameKind::kDialog;
}

}  // namespace chromeos
