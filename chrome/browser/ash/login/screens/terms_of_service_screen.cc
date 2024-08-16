// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/terms_of_service_screen.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr const char kAccept[] = "accept";
constexpr const char kBack[] = "back";
constexpr const char kRetry[] = "retry";
constexpr const char kUserTos[] = "user_managed_terms_of_service.txt";

// This allows to set callback before screen is created.
base::OnceClosure& GetTosSavedCallbackOverride() {
  static base::NoDestructor<base::OnceClosure> tos_saved_for_testing;
  return *tos_saved_for_testing;
}

void SaveTosToFile(const std::string& tos, const base::FilePath& tos_path) {
  if (!base::ImportantFileWriter::WriteFileAtomically(tos_path, tos)) {
    LOG(ERROR) << "Failed to save terms of services to file: "
               << tos_path.AsUTF8Unsafe();
  }
}

std::optional<std::string> ReadFileToOptionalString(
    const base::FilePath& file_path) {
  std::string content;
  if (base::ReadFileToString(file_path, &content))
    return std::make_optional<std::string>(content);
  return std::nullopt;
}

}  // namespace

// static
std::string TermsOfServiceScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::ACCEPTED:
      return "Accepted";
    case Result::DECLINED:
      return "Declined";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

TermsOfServiceScreen::TermsOfServiceScreen(
    base::WeakPtr<TermsOfServiceScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(TermsOfServiceScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

TermsOfServiceScreen::~TermsOfServiceScreen() = default;

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

bool TermsOfServiceScreen::MaybeSkip(WizardContext& context) {
  // Only show the Terms of Service when Terms of Service have been specified
  // through policy. In all other cases, advance to the post-ToS part
  // immediately.
  if (context.skip_post_login_screens_for_tests ||
      !ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          prefs::kTermsOfServiceURL)) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  if (user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession()) {
    return false;
  }

  return false;
}

void TermsOfServiceScreen::ShowImpl() {
  if (!view_)
    return;

  // Set the domain name whose Terms of Service are being shown.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  // Show the screen.
  view_->Show(
      ash::InstallAttributes::Get()->IsEnterpriseManaged()
          ? connector->GetEnterpriseDomainManager()
          : enterprise_util::GetDomainFromEmail(
                ProfileManager::GetActiveUserProfile()->GetProfileUserName()));

  // Start downloading the Terms of Service.
  StartDownload();
}

void TermsOfServiceScreen::HideImpl() {}

void TermsOfServiceScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kBack)
    OnDecline();
  else if (action_id == kAccept)
    OnAccept();
  else if (action_id == kRetry)
    OnRetry();
  else
    BaseScreen::OnUserAction(args);
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
  download_timer_.Start(FROM_HERE, base::Minutes(1), this,
                        &TermsOfServiceScreen::OnDownloadTimeout);
}

void TermsOfServiceScreen::OnDownloadTimeout() {
  // Destroy the fetcher, which will abort the download attempt.
  terms_of_service_loader_.reset();

  LoadFromFileOrShowError();
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
  // text/plain or are empty, try to load offline version or show an error
  // message to the user.
  if (!response_body || *response_body == "" || !loader->ResponseInfo() ||
      loader->ResponseInfo()->mime_type != "text/plain") {
    LoadFromFileOrShowError();
  } else {
    // If the Terms of Service were downloaded successfully, sanitize and show
    // them to the user.
    view_->OnLoadSuccess(base::EscapeForHTML(*response_body));
    // Update locally saved terms.
    SaveTos(base::EscapeForHTML(*response_body));
  }
}

void TermsOfServiceScreen::LoadFromFileOrShowError() {
  if (!view_)
    return;
  auto tos_path = GetTosFilePath();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadFileToOptionalString, tos_path),
      base::BindOnce(&TermsOfServiceScreen::OnTosLoadedFromFile,
                     weak_factory_.GetWeakPtr()));
}

void TermsOfServiceScreen::OnTosLoadedFromFile(std::optional<std::string> tos) {
  if (!view_)
    return;
  if (!tos.has_value()) {
    view_->OnLoadError();
    return;
  }
  view_->OnLoadSuccess(tos.value());
}

// static
void TermsOfServiceScreen::SetTosSavedCallbackForTesting(
    base::OnceClosure callback) {
  GetTosSavedCallbackOverride() = std::move(callback);
}

// static
base::FilePath TermsOfServiceScreen::GetTosFilePath() {
  auto user_data_dir = ProfileManager::GetActiveUserProfile()->GetPath();
  return user_data_dir.AppendASCII(kUserTos);
}

void TermsOfServiceScreen::SaveTos(const std::string& tos) {
  auto tos_path = GetTosFilePath();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&SaveTosToFile, tos, tos_path),
      base::BindOnce(&TermsOfServiceScreen::OnTosSavedForTesting,
                     weak_factory_.GetWeakPtr()));
}

void TermsOfServiceScreen::OnTosSavedForTesting() {
  if (GetTosSavedCallbackOverride())
    std::move(GetTosSavedCallbackOverride()).Run();
}

}  // namespace ash
