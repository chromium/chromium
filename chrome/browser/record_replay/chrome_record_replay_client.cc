// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/chrome_record_replay_client.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/record_replay/recording_data_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/record_replay/content/browser/content_record_replay_driver.h"
#include "components/record_replay/content/browser/content_record_replay_driver_factory.h"
#include "components/record_replay/core/browser/record_replay_driver.h"
#include "components/record_replay/core/browser/recording_data_manager.h"
#include "components/record_replay/core/browser/task_discovery_service.h"
#include "components/record_replay/core/browser/task_discovery_service_impl.h"
#include "components/record_replay/core/common/record_replay.mojom.h"
#include "components/record_replay/core/common/record_replay_features.h"
#include "content/public/browser/navigation_handle.h"
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
    : ChromeRecordReplayClient(
          tab,
          std::make_unique<record_replay::TaskDiscoveryServiceImpl>()) {}

ChromeRecordReplayClient::ChromeRecordReplayClient(
    tabs::TabInterface& tab,
    std::unique_ptr<record_replay::TaskDiscoveryService> service)
    : tabs::ContentsObservingTabFeature(tab),
      task_discovery_service_(std::move(service)) {
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
  record_replay::ContentRecordReplayDriverFactory& driver_factory =
      static_cast<ChromeRecordReplayClient*>(client)->driver_factory_;
  record_replay::ContentRecordReplayDriver* driver =
      driver_factory.GetOrCreateDriver(rfh);
  if (!driver) {
    return;
  }
  static_cast<record_replay::ContentRecordReplayDriver*>(driver)
      ->BindPendingReceiver(std::move(pending_receiver));
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

void ChromeRecordReplayClient::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  task_discovery_service_->ShouldOfferTask(
      navigation_handle->GetURL(),
      base::BindOnce(&ChromeRecordReplayClient::OnShouldOfferTask,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChromeRecordReplayClient::OnShouldOfferTask(bool offered) {
  if (!offered) {
    return;
  }

  if (glic::GlicNudgeController* nudge_controller =
          tab()
              .GetBrowserWindowInterface()
              ->GetFeatures()
              .glic_nudge_controller()) {
    std::optional<record_replay::TaskDiscoveryService::AutomationMetadata>
        metadata = task_discovery_service_->GetMetadata();
    if (metadata.has_value()) {
      nudge_controller->UpdateNudgeLabel(
          tab().GetContents(), metadata->title,
          std::make_optional(metadata->instructions),
          metadata->anchored_message, /*task=*/std::nullopt, base::DoNothing());
    }
  }
}
