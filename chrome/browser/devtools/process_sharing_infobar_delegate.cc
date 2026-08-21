// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/process_sharing_infobar_delegate.h"

#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/infobars/core/infobar.h"
#include "components/webui/flags/pref_service_flags_storage.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace {

void OptOutAndRestart(base::WeakPtr<content::WebContents> web_contents) {
#if BUILDFLAG(IS_CHROMEOS)
  PrefService* prefs =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();
#else
  PrefService* prefs = g_browser_process->local_state();
#endif
  // Note: Both ChromeOS owner and non-owner use PrefServiceFlagsStorage
  // under the hood. OwnersFlagsStorage has additional functionalities
  // for setting flags but since we are just reading the storage assume
  // non-owner case and bypass asynchronous owner check.
  auto flags_storage =
      std::make_unique<flags_ui::PrefServiceFlagsStorage>(prefs);

  about_flags::SetFeatureEntryEnabled(
      flags_storage.get(),
      "enable-process-per-site-up-to-main-frame-threshold@2", true);
  flags_storage->CommitPendingWrites();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::AttemptRestart));
}

void ShowRestartDialog(content::WebContents* web_contents) {
  constrained_window::ShowBrowserModal(
      ui::DialogModel::Builder()
          .SetInternalName("ProcessSharingAppRestart")
          .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
              IDS_DEV_TOOLS_SHARED_PROCESS_INFOBAR_RESTART_NEEDED)))
          .AddOkButton(
              base::BindOnce(&OptOutAndRestart, web_contents->GetWeakPtr()))
          .AddCancelButton(base::DoNothing())
          .Build(),
      web_contents->GetTopLevelNativeWindow());
}

}  // namespace

ProcessSharingInfobarDelegate::ProcessSharingInfobarDelegate(
    content::WebContents* web_contents)
    : inspected_web_contents_(web_contents->GetWeakPtr()) {}

ProcessSharingInfobarDelegate::~ProcessSharingInfobarDelegate() = default;

std::u16string ProcessSharingInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_DEV_TOOLS_SHARED_PROCESS_INFOBAR);
}

int ProcessSharingInfobarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string ProcessSharingInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(
      IDS_DEV_TOOLS_SHARED_PROCESS_INFOBAR_OPT_OUT);
}

std::u16string ProcessSharingInfobarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(
      IDS_DEV_TOOLS_SHARED_PROCESS_INFOBAR_LEARN_MORE);
}

GURL ProcessSharingInfobarDelegate::GetLinkURL() const {
  return GURL("https://developer.chrome.com/blog/process-sharing-experiment");
}

infobars::InfoBarDelegate::InfoBarIdentifier
ProcessSharingInfobarDelegate::GetIdentifier() const {
  return DEV_TOOLS_SHARED_PROCESS_DELEGATE;
}

bool ProcessSharingInfobarDelegate::Accept() {
  if (inspected_web_contents_) {
    ShowRestartDialog(inspected_web_contents_.get());
  }

  return ConfirmInfoBarDelegate::Accept();
}
