// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/chrome_record_replay_client.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/record_replay/record_replay_driver.h"
#include "chrome/browser/record_replay/record_replay_driver_factory.h"
#include "chrome/browser/record_replay/recording_data_manager.h"
#include "chrome/browser/record_replay/recording_data_manager_factory.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/common/record_replay/record_replay.mojom.h"
#include "chrome/common/record_replay/record_replay_features.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace {

GURL StripAuthAndParams(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

}  // namespace

DEFINE_USER_DATA(ChromeRecordReplayClient);

ChromeRecordReplayClient::ChromeRecordReplayClient(tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab) {
  CHECK(
      base::FeatureList::IsEnabled(record_replay::features::kRecordReplayBase));
  driver_factory_.Observe(tab.GetContents());
}

ChromeRecordReplayClient::~ChromeRecordReplayClient() = default;

void ChromeRecordReplayClient::OnDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  driver_factory_.Observe(new_contents);
}

// static
void ChromeRecordReplayClient::BindRecordReplayDriver(
    content::RenderFrameHost* rfh,
    mojo::PendingAssociatedReceiver<record_replay::mojom::RecordReplayDriver>
        pending_receiver) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  RecordReplayClient* client = tab->GetTabFeatures()->record_replay_client();
  if (!client) {
    return;
  }
  record_replay::RecordReplayDriver* driver =
      client->GetDriverFactory().GetOrCreateDriver(rfh);
  if (!driver) {
    return;
  }
  driver->BindPendingReceiver(std::move(pending_receiver));
}

record_replay::RecordReplayManager& ChromeRecordReplayClient::GetManager() {
  return manager_;
}

record_replay::RecordReplayDriverFactory&
ChromeRecordReplayClient::GetDriverFactory() {
  return driver_factory_;
}

record_replay::RecordingDataManager*
ChromeRecordReplayClient::GetRecordingDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(tab().GetContents()->GetBrowserContext());
  return record_replay::RecordingDataManagerFactory::GetForProfile(profile);
}

GURL ChromeRecordReplayClient::GetPrimaryMainFrameUrl() {
  return StripAuthAndParams(tab().GetContents()->GetLastCommittedURL());
}

autofill::AutofillClient* ChromeRecordReplayClient::GetAutofillClient() {
  return autofill::ContentAutofillClient::FromWebContents(tab().GetContents());
}

void ChromeRecordReplayClient::ReportToUser(std::string_view message) {
  ToastController* const toast_controller =
      ToastController::MaybeGetForWebContents(tab().GetContents());
  if (toast_controller) {
    ToastParams params(ToastId::kRecordReplay);
    params.body_string_override = base::UTF8ToUTF16(message);
    toast_controller->MaybeShowToast(std::move(params));
  }
}
