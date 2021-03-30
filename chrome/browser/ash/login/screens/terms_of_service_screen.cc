// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/terms_of_service_screen.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

constexpr const char kAccept[] = "accept";
constexpr const char kBack[] = "back";
constexpr const char kRetry[] = "retry";

}  // namespace

// static
std::string TermsOfServiceScreen::GetResultString(Result result) {
  switch (result) {
    case Result::ACCEPTED:
      return "Accepted";
    case Result::DECLINED:
      return "Declined";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

TermsOfServiceScreen::TermsOfServiceScreen(
    TermsOfServiceScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(TermsOfServiceScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetScreen(this);
}

TermsOfServiceScreen::~TermsOfServiceScreen() {
  if (view_)
    view_->SetScreen(nullptr);
}

void TermsOfServiceScreen::OnDecline() {
  exit_callback_.Run(Result::DECLINED);
}

void TermsOfServiceScreen::OnAccept() {
  if (view_ && view_->AreTermsLoaded()) {
    exit_callback_.Run(Result::ACCEPTED);
    return;
  }
  // If the Terms of Service have not been successfully downloaded, the "accept
  // and continue" button should not be accessible. If the user managed to
  // activate it somehow anyway, do not treat this as acceptance of the Terms
  // and Conditions and end the session instead, as if the user had declined.
  OnDecline();
}

void TermsOfServiceScreen::OnRetry() {
  // If the Terms of Service have been successfully downloaded or are still
  // being downloaded, this button should not be accessible. If the user managed
  // to activate it somehow anyway, do not do anything.
  if (view_ && view_->AreTermsLoaded())
    return;
  if (terms_of_service_loader_)
    return;

  StartDownload();
}

void TermsOfServiceScreen::OnViewDestroyed(TermsOfServiceScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool TermsOfServiceScreen::MaybeSkip(WizardContext* context) {
  // Only show the Terms of Service when logging into a public account and Terms
  // of Service have been specified through policy. In all other cases, advance
  // to the post-ToS part immediately.
  if (!user_manager::UserManager::Get()->IsLoggedInAsPublicAccount() ||
      !ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          prefs::kTermsOfServiceURL)) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void TermsOfServiceScreen::ShowImpl() {
  if (!view_)
    return;

  // Set the domain name whose Terms of Service are being shown.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  view_->SetManager(connector->GetEnterpriseDomainManager());

  // Show the screen.
  view_->Show();

  // Start downloading the Terms of Service.
  StartDownload();
}

void TermsOfServiceScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void TermsOfServiceScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kBack)
    OnDecline();
  else if (action_id == kAccept)
    OnAccept();
  else if (action_id == kRetry)
    OnRetry();
  else
    BaseScreen::OnUserAction(action_id);
}

void TermsOfServiceScreen::StartDownload() {
  const PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  // If an URL from which the Terms of Service can be downloaded has not been
  // set, show an error message to the user.
  std::string terms_of_service_url =
      prefs->GetString(prefs::kTermsOfServiceURL);
  if (terms_of_service_url.empty()) {
    if (view_)
      view_->OnLoadError();
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("terms_of_service_fetch", R"(
          semantics {
            sender: "Chrome OS Terms of Service"
            description:
              "Chrome OS downloads the latest terms of service document "
              "from Google servers."
            trigger:
              "When a public session starts on managed devices and an admin "
              "has uploaded a terms of service document for the domain."
            data:
              "URL of the terms of service document. "
              "No user information is sent."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "user"
            setting: "Unconditionally enabled on Chrome OS."
            policy_exception_justification:
              "Not implemented, considered not useful."
          })");
  // Start downloading the Terms of Service.

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(terms_of_service_url);
  // Request a text/plain MIME type as only plain-text Terms of Service are
  // accepted.
  resource_request->headers.SetHeader("Accept", "text/plain");
  terms_of_service_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Retry up to three times if network changes are detected during the
  // download.
  terms_of_service_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  network::mojom::URLLoaderFactory* loader_factory =
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory();
  terms_of_service_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, base::BindOnce(&TermsOfServiceScreen::OnDownloaded,
                                     base::Unretained(this)));

  // Abort the download attempt if it takes longer than one minute.
  download_timer_.Start(FROM_HERE, base::TimeDelta::FromMinutes(1), this,
                        &TermsOfServiceScreen::OnDownloadTimeout);
}

void TermsOfServiceScreen::OnDownloadTimeout() {
  // Destroy the fetcher, which will abort the download attempt.
  terms_of_service_loader_.reset();

  // Show an error message to the user.
  if (view_)
    view_->OnLoadError();
}

void TermsOfServiceScreen::OnDownloaded(
    std::unique_ptr<std::string> response_body) {
  download_timer_.Stop();

  // Destroy the fetcher when this method returns.
  std::unique_ptr<network::SimpleURLLoader> loader(
      std::move(terms_of_service_loader_));
  if (!view_)
    return;

  // If the Terms of Service could not be downloaded, do not have a MIME type of
  // text/plain or are empty, show an error message to the user.
  if (!response_body || *response_body == "" || !loader->ResponseInfo() ||
      loader->ResponseInfo()->mime_type != "text/plain") {
    view_->OnLoadError();
  } else {
    // If the Terms of Service were downloaded successfully, show them to the
    // user.
    view_->OnLoadSuccess(*response_body);
  }
}

}  // namespace chromeos
