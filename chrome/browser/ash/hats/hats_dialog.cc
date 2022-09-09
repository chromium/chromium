// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/hats/hats_dialog.h"

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
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
#include "chromeos/version/version_loader.h"
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

}  // namespace

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

HatsDialog::HatsDialog(const std::string& trigger_id,
                       const std::string& histogram_name,
                       const std::string& site_context)
    : trigger_id_(trigger_id), histogram_name_(histogram_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  url_ = std::string(kCrOSHaTSURL) + "?" + site_context +
         "&trigger=" + trigger_id_;
  set_can_resize(false);
}

void HatsDialog::Show(const std::string& trigger_id,
                      const std::string& histogram_name,
                      const std::string& site_context) {
  // HatsDialog is self-deleting via OnDialogClosed().
  chrome::ShowWebDialog(
      nullptr, ProfileManager::GetActiveUserProfile(),
      new HatsDialog(trigger_id, histogram_name, site_context));
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

void HatsDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

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
