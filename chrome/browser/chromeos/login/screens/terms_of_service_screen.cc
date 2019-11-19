// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
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

TermsOfServiceScreen::TermsOfServiceScreen(
    TermsOfServiceScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(TermsOfServiceScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

TermsOfServiceScreen::~TermsOfServiceScreen() {
  if (view_)
    view_->SetDelegate(NULL);
}

void TermsOfServiceScreen::OnDecline() {
  exit_callback_.Run(Result::DECLINED);
}

void TermsOfServiceScreen::OnAccept() {
  exit_callback_.Run(Result::ACCEPTED);
}

void TermsOfServiceScreen::OnViewDestroyed(TermsOfServiceScreenView* view) {
  if (view_ == view)
    view_ = NULL;
}

void TermsOfServiceScreen::Show() {
  if (!view_)
    return;

  // Set the domain name whose Terms of Service are being shown.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  view_->SetDomain(connector->GetEnterpriseDisplayDomain());

  // Show the screen.
  view_->Show();

  // Start downloading the Terms of Service.
  StartDownload();
}

void TermsOfServiceScreen::Hide() {
  if (view_)
    view_->Hide();
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
