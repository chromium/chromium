// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_dialog.h"

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_finch_helper.h"
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
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace ash {

namespace {

// Default width/height ratio of screen size.
const int kDefaultWidth = 384;
const int kDefaultHeight = 428;

// There are 5 possible choices, from very_dissatisfied to very_satisfied.
const int kMaxFeedbackScore = 5;

// Possible requested actions from the HTML+JS client.
// Client is ready to close the page.
const char kClientActionClose[] = "close";
// There was an unhandled error and we need to log and close the page.
const char kClientActionUnhandledError[] = "survey-loading-error";
// A smiley was selected, so we'd like to track that.
const char kClientSmileySelected[] = "smiley-selected-";

constexpr char kCrOSHaTSURL[] =
    "https://storage.googleapis.com/chromeos-hats-web-stable/index.html";

// Delimiters used to join the separate device info elements into a single
// string to be used as site context.
const char kDeviceInfoStopKeyword[] = "&";
const char kDeviceInfoKeyValueDelimiter[] = "=";
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

}  // namespace

// static
std::string HatsDialog::GetFormattedSiteContext(
    const std::string& user_locale,
    const base::flat_map<std::string, std::string>& product_specific_data) {
  base::flat_map<std::string, std::string> context;

  context[KeyEnumToString(DeviceInfoKey::BROWSER)] =
      version_info::GetVersionNumber();

  context[KeyEnumToString(DeviceInfoKey::PLATFORM)] =
      version_loader::GetVersion(version_loader::VERSION_FULL);

  context[KeyEnumToString(DeviceInfoKey::FIRMWARE)] =
      version_loader::GetFirmware();

  context[KeyEnumToString(DeviceInfoKey::LOCALE)] = user_locale;

  for (const auto& pair : context) {
    if (product_specific_data.contains(pair.first)) {
      LOG(WARNING) << "Product specific data contains reserved key "
                   << pair.first << ". Value will be overwritten.";
    }
  }
  context.insert(product_specific_data.begin(), product_specific_data.end());

  std::stringstream stream;
  bool first_iteration = true;
  for (const auto& pair : context) {
    if (!first_iteration)
      stream << kDeviceInfoStopKeyword;

    stream << net::EscapeQueryParamValue(pair.first, /*use_plus=*/false)
           << kDeviceInfoKeyValueDelimiter
           << net::EscapeQueryParamValue(pair.second, /*use_plus=*/false);

    first_iteration = false;
  }
  return stream.str();
}

// static
bool HatsDialog::HandleClientTriggeredAction(
    const std::string& action,
    const std::string& histogram_name) {
  DVLOG(1) << "HandleClientTriggeredAction: Received " << action;

  // Page asks to be closed.
  if (action == kClientActionClose) {
    return true;
  }
  // An unhandled error in our client, log and close.
  if (base::StartsWith(action, kClientActionUnhandledError)) {
    LOG(ERROR) << "Error while loading a HaTS Survey " << action;
    return true;
  }
  // A Smiley (score) was selected.
  if (base::StartsWith(action, kClientSmileySelected)) {
    int score;
    if (!base::StringToInt(action.substr(strlen(kClientSmileySelected)),
                           &score)) {
      LOG(ERROR) << "Can't parse Survey score";
      return false;  // It's a client error, but don't close the page.
    }
    DVLOG(1) << "Setting UMA Metric for smiley " << score;
    base::UmaHistogramExactLinear(histogram_name, score, kMaxFeedbackScore + 1);
    return false;  // Don't close the page.
  }

  // Future proof - ignore unimplemented commands.
  return false;
}

// static
std::unique_ptr<HatsDialog> HatsDialog::CreateAndShow(
    const HatsConfig& hats_config,
    const base::flat_map<std::string, std::string>& product_specific_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::string user_locale =
      profile->GetPrefs()->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&user_locale);
  if (!user_locale.length())
    user_locale = kDefaultProfileLocale;

  std::unique_ptr<HatsDialog> hats_dialog(
      new HatsDialog(HatsFinchHelper::GetTriggerID(hats_config), profile,
                     hats_config.histogram_name));

  // Raw pointer is used here since the dialog is owned by the hats
  // notification controller which lives until the end of the user session. The
  // dialog will always be closed before that time instant.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetFormattedSiteContext, user_locale,
                     product_specific_data),
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

HatsDialog::HatsDialog(const std::string& trigger_id,
                       Profile* user_profile,
                       const std::string& histogram_name)
    : trigger_id_(trigger_id),
      user_profile_(user_profile),
      histogram_name_(histogram_name) {
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

std::u16string HatsDialog::GetDialogTitle() const {
  return std::u16string();
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

void HatsDialog::OnCloseContents(WebContents* source, bool* out_close_dialog) {
  *out_close_dialog = true;
}

void HatsDialog::OnDialogClosed(const std::string& json_retval) {}

void HatsDialog::OnLoadingStateChanged(WebContents* source) {
  const std::string ref = source->GetURL().ref();

  if (HandleClientTriggeredAction(ref, histogram_name_)) {
    source->ClosePage();
  }
}

bool HatsDialog::ShouldShowDialogTitle() const {
  return false;
}

bool HatsDialog::ShouldShowCloseButton() const {
  return true;
}

bool HatsDialog::HandleContextMenu(content::RenderFrameHost& render_frame_host,
                                   const content::ContextMenuParams& params) {
  // Disable context menu
  return true;
}

ui::WebDialogDelegate::FrameKind HatsDialog::GetWebDialogFrameKind() const {
  return ui::WebDialogDelegate::FrameKind::kDialog;
}

}  // namespace ash
