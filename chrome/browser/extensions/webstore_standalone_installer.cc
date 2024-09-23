// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_standalone_installer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/scoped_active_install.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "url/gurl.h"

using content::WebContents;

namespace {
constexpr char kProfileShuttingDown[] = "The profile is shutting down.";
}

namespace extensions {

WebstoreStandaloneInstaller::WebstoreStandaloneInstaller(
    const std::string& webstore_item_id,
    Profile* profile,
    Callback callback)
    : id_(webstore_item_id), callback_(std::move(callback)), profile_(profile) {
  observation_.Observe(profile);
}

void WebstoreStandaloneInstaller::BeginInstall() {
  // Add a ref to keep this alive for WebstoreDataFetcher.
  // All code paths from here eventually lead to either CompleteInstall or
  // AbortInstall, which both call CleanUp to release this ref.
  AddRef();

  if (!crx_file::id_util::IdIsValid(id_)) {
    CompleteInstall(webstore_install::INVALID_ID,
                    webstore_install::kInvalidWebstoreItemId);
    return;
  }

  webstore_install::Result result = webstore_install::OTHER_ERROR;
  std::string error;
  if (!EnsureUniqueInstall(&result, &error)) {
    CompleteInstall(result, error);
    return;
  }

  // Use the requesting page as the referrer both since that is more correct
  // (it is the page that caused this request to happen) and so that we can
  // track top sites that trigger inline install requests.
  webstore_data_fetcher_ =
      std::make_unique<WebstoreDataFetcher>(this, GURL(), id_);

  webstore_data_fetcher_->Start(profile_->GetDefaultStoragePartition()
                                    ->GetURLLoaderFactoryForBrowserProcess()
                                    .get());
}

//
// Private interface implementation.
//

WebstoreStandaloneInstaller::~WebstoreStandaloneInstaller() {
}

void WebstoreStandaloneInstaller::RunCallback(bool success,
                                              const std::string& error,
                                              webstore_install::Result result) {
  DCHECK(callback_);
  std::move(callback_).Run(success, error, result);
}

void WebstoreStandaloneInstaller::AbortInstall() {
  callback_.Reset();
  // Abort any in-progress fetches.
  if (webstore_data_fetcher_) {
    webstore_data_fetcher_.reset();
    scoped_active_install_.reset();
  }
  CleanUp();
}

bool WebstoreStandaloneInstaller::EnsureUniqueInstall(
    webstore_install::Result* reason,
    std::string* error) {
  InstallTracker* tracker = InstallTracker::Get(profile_);
  DCHECK(tracker);

  const ActiveInstallData* existing_install_data =
      tracker->GetActiveInstall(id_);
  if (existing_install_data) {
    *reason = webstore_install::INSTALL_IN_PROGRESS;
    *error = webstore_install::kInstallInProgressError;
    return false;
  }

  ActiveInstallData install_data(id_);
  scoped_active_install_ =
      std::make_unique<ScopedActiveInstall>(tracker, install_data);
  return true;
}

void WebstoreStandaloneInstaller::CompleteInstall(
    webstore_install::Result result,
    const std::string& error) {
  scoped_active_install_.reset();
  if (!callback_.is_null())
    RunCallback(result == webstore_install::SUCCESS, error, result);
  CleanUp();
}

void WebstoreStandaloneInstaller::ProceedWithInstallPrompt() {
  install_prompt_ = CreateInstallPrompt();
  if (install_prompt_.get()) {
    ShowInstallUI();
    // Control flow finishes up in OnInstallPromptDone().
  } else {
    OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload(
        ExtensionInstallPrompt::Result::ACCEPTED));
  }
}

scoped_refptr<const Extension>
WebstoreStandaloneInstaller::GetLocalizedExtensionForDisplay() {
  if (!localized_extension_for_display_.get()) {
    DCHECK(manifest_.has_value());
    if (!manifest_.has_value())
      return nullptr;

    std::string error;
    localized_extension_for_display_ =
        ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
            *manifest_, Extension::REQUIRE_KEY | Extension::FROM_WEBSTORE, id_,
            localized_name_, localized_description_, &error);
  }
  return localized_extension_for_display_.get();
}

void WebstoreStandaloneInstaller::OnManifestParsed() {
  ProceedWithInstallPrompt();
}

std::unique_ptr<ExtensionInstallPrompt>
WebstoreStandaloneInstaller::CreateInstallUI() {
  return std::make_unique<ExtensionInstallPrompt>(GetWebContents());
}

std::unique_ptr<WebstoreInstaller::Approval>
WebstoreStandaloneInstaller::CreateApproval() const {
  std::unique_ptr<WebstoreInstaller::Approval> approval(
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          profile_, id_, manifest_->Clone(), true));
  approval->skip_post_install_ui = !ShouldShowPostInstallUI();
  approval->installing_icon = gfx::ImageSkia::CreateFrom1xBitmap(icon_);
  return approval;
}

void WebstoreStandaloneInstaller::OnInstallPromptDone(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  if (payload.result == ExtensionInstallPrompt::Result::USER_CANCELED) {
    CompleteInstall(webstore_install::USER_CANCELLED,
                    webstore_install::kUserCancelledError);
    return;
  }

  if (payload.result == ExtensionInstallPrompt::Result::ABORTED ||
      !CheckRequestorAlive()) {
    CompleteInstall(webstore_install::ABORTED, std::string());
    return;
  }

  DCHECK(payload.result == ExtensionInstallPrompt::Result::ACCEPTED);

  std::unique_ptr<WebstoreInstaller::Approval> approval = CreateApproval();

  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  const Extension* installed_extension =
      extension_registry->GetExtensionById(id_, ExtensionRegistry::EVERYTHING);
  if (installed_extension) {
    std::string install_message;
    webstore_install::Result install_result = webstore_install::SUCCESS;

    ExtensionService* extension_service =
        ExtensionSystem::Get(profile_)->extension_service();
    if (blocklist_prefs::IsExtensionBlocklisted(
            id_, ExtensionPrefs::Get(profile_))) {
      // Don't install a blocklisted extension.
      install_result = webstore_install::BLOCKLISTED;
      install_message = webstore_install::kExtensionIsBlocklisted;
    } else if (!extension_service->IsExtensionEnabled(id_)) {
      // If the extension is installed but disabled, and not blocklisted,
      // enable it.
      extension_service->EnableExtension(id_);
    }  // else extension is installed and enabled; no work to be done.

    CompleteInstall(install_result, install_message);
    return;
  }

  auto installer = base::MakeRefCounted<WebstoreInstaller>(
      profile_,
      base::BindOnce(&WebstoreStandaloneInstaller::OnExtensionInstallSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WebstoreStandaloneInstaller::OnExtensionInstallFailure,
                     weak_ptr_factory_.GetWeakPtr()),
      GetWebContents(), id_, std::move(approval), install_source_);
  installer->Start();
}

void WebstoreStandaloneInstaller::OnWebstoreRequestFailure(
    const std::string& extension_id) {
  OnWebStoreDataFetcherDone();
  CompleteInstall(webstore_install::WEBSTORE_REQUEST_ERROR,
                  webstore_install::kWebstoreRequestError);
}

void WebstoreStandaloneInstaller::OnWebstoreItemJSONAPIResponseParseSuccess(
    const std::string& extension_id,
    const base::Value::Dict& webstore_data) {
  OnWebStoreDataFetcherDone();

  if (!CheckRequestorAlive()) {
    CompleteInstall(webstore_install::ABORTED, std::string());
    return;
  }

  std::optional<double> average_rating_setting =
      webstore_data.FindDouble(kAverageRatingKey);
  std::optional<int> rating_count_setting =
      webstore_data.FindInt(kRatingCountKey);

  // Manifest, number of users, average rating and rating count are required.
  const std::string* manifest = webstore_data.FindString(kManifestKey);
  const std::string* localized_user_count = webstore_data.FindString(kUsersKey);
  if (!manifest || !localized_user_count || !average_rating_setting ||
      !rating_count_setting) {
    CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                    webstore_install::kInvalidWebstoreResponseError);
    return;
  }
  localized_user_count_ = *localized_user_count;

  average_rating_ = *average_rating_setting;
  rating_count_ = *rating_count_setting;
  localized_rating_count_ = base::NumberToString(rating_count_);

  // Showing user count is optional.
  std::optional<bool> show_user_count_opt =
      webstore_data.FindBool(kShowUserCountKey);
  show_user_count_ = show_user_count_opt.value_or(true);

  if (average_rating_ < ExtensionInstallPrompt::kMinExtensionRating ||
      average_rating_ > ExtensionInstallPrompt::kMaxExtensionRating) {
    CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                    webstore_install::kInvalidWebstoreResponseError);
    return;
  }

  // Localized name and description are optional.
  bool ok = true;
  if (const base::Value* localized_name_in =
          webstore_data.Find(kLocalizedNameKey)) {
    if (localized_name_in->is_string())
      localized_name_ = localized_name_in->GetString();
    else
      ok = false;
  }

  if (const base::Value* localized_description_in =
          webstore_data.Find(kLocalizedDescriptionKey)) {
    if (localized_description_in->is_string())
      localized_description_ = localized_description_in->GetString();
    else
      ok = false;
  }

  if (!ok) {
    CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                    webstore_install::kInvalidWebstoreResponseError);
    return;
  }

  // Icon URL is optional.
  GURL icon_url;
  if (const base::Value* icon_url_val = webstore_data.Find(kIconUrlKey)) {
    const std::string* icon_url_string = icon_url_val->GetIfString();
    if (!icon_url_string) {
      CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                      webstore_install::kInvalidWebstoreResponseError);
      return;
    }
    icon_url = extension_urls::GetWebstoreLaunchURL().Resolve(*icon_url_string);
    if (!icon_url.is_valid()) {
      CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                      webstore_install::kInvalidWebstoreResponseError);
      return;
    }
  }

  auto helper = base::MakeRefCounted<WebstoreInstallHelper>(
      this, id_, *manifest, icon_url);

  // The helper will call us back via OnWebstoreParseSuccess() or
  // OnWebstoreParseFailure().
  helper->Start(profile_->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());
}

void WebstoreStandaloneInstaller::OnFetchItemSnippetParseSuccess(
    const std::string& extension_id,
    FetchItemSnippetResponse item_snippet) {
  OnWebStoreDataFetcherDone();
  if (!CheckRequestorAlive()) {
    CompleteInstall(webstore_install::ABORTED, std::string());
    return;
  }

  rating_count_ = base::checked_cast<int>(item_snippet.rating_count());
  localized_rating_count_ = item_snippet.rating_count_string();
  average_rating_ = item_snippet.average_rating();
  localized_user_count_ = item_snippet.user_count_string();
  show_user_count_ = !localized_user_count_.empty();
  localized_name_ = item_snippet.title();
  localized_description_ = item_snippet.summary();

  GURL icon_url =
      extension_urls::GetWebstoreLaunchURL().Resolve(item_snippet.logo_uri());
  if (!icon_url.is_valid()) {
    CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE,
                    webstore_install::kInvalidWebstoreResponseError);
    return;
  }

  auto helper = base::MakeRefCounted<WebstoreInstallHelper>(
      this, id_, item_snippet.manifest(), icon_url);

  // The helper will call us back via OnWebstoreParseSuccess() or
  // OnWebstoreParseFailure().
  helper->Start(profile_->GetDefaultStoragePartition()
                    ->GetURLLoaderFactoryForBrowserProcess()
                    .get());
}

void WebstoreStandaloneInstaller::OnWebstoreResponseParseFailure(
    const std::string& extension_id,
    const std::string& error) {
  OnWebStoreDataFetcherDone();
  CompleteInstall(webstore_install::INVALID_WEBSTORE_RESPONSE, error);
}

void WebstoreStandaloneInstaller::OnWebstoreParseSuccess(
    const std::string& id,
    const SkBitmap& icon,
    base::Value::Dict manifest) {
  CHECK_EQ(id_, id);

  if (!CheckRequestorAlive()) {
    CompleteInstall(webstore_install::ABORTED, std::string());
    return;
  }

  manifest_ = std::move(manifest);
  icon_ = icon;

  OnManifestParsed();
}

void WebstoreStandaloneInstaller::OnWebstoreParseFailure(
    const std::string& id,
    InstallHelperResultCode result_code,
    const std::string& error_message) {
  webstore_install::Result install_result = webstore_install::OTHER_ERROR;
  switch (result_code) {
    case WebstoreInstallHelper::Delegate::kManifestError:
      install_result = webstore_install::INVALID_MANIFEST;
      break;
    case WebstoreInstallHelper::Delegate::ICON_ERROR:
      install_result = webstore_install::ICON_ERROR;
      break;
    default:
      break;
  }

  CompleteInstall(install_result, error_message);
}

void WebstoreStandaloneInstaller::OnExtensionInstallSuccess(
    const std::string& id) {
  CHECK_EQ(id_, id);
  CompleteInstall(webstore_install::SUCCESS, std::string());
}

void WebstoreStandaloneInstaller::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason reason) {
  CHECK_EQ(id_, id);

  webstore_install::Result install_result = webstore_install::OTHER_ERROR;
  switch (reason) {
    case WebstoreInstaller::FAILURE_REASON_CANCELLED:
      install_result = webstore_install::USER_CANCELLED;
      break;
    case WebstoreInstaller::FAILURE_REASON_DEPENDENCY_NOT_FOUND:
    case WebstoreInstaller::FAILURE_REASON_DEPENDENCY_NOT_SHARED_MODULE:
      install_result = webstore_install::MISSING_DEPENDENCIES;
      break;
    default:
      break;
  }

  CompleteInstall(install_result, error);
}

void WebstoreStandaloneInstaller::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK(profile == profile_);

  if (!callback_.is_null())
    RunCallback(false, kProfileShuttingDown, webstore_install::ABORTED);

  AbortInstall();
}

void WebstoreStandaloneInstaller::ShowInstallUI() {
  scoped_refptr<const Extension> localized_extension =
      GetLocalizedExtensionForDisplay();
  if (!localized_extension.get()) {
    CompleteInstall(webstore_install::INVALID_MANIFEST,
                    webstore_install::kInvalidManifestError);
    return;
  }

  install_ui_ = CreateInstallUI();
  install_ui_->ShowDialog(
      base::BindOnce(&WebstoreStandaloneInstaller::OnInstallPromptDone, this),
      localized_extension.get(), &icon_, std::move(install_prompt_),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
}

void WebstoreStandaloneInstaller::OnWebStoreDataFetcherDone() {
  // An instance of this class is passed in as a delegate for the
  // WebstoreInstallHelper, ExtensionInstallPrompt and WebstoreInstaller, and
  // therefore needs to remain alive until they are done. Clear the webstore
  // data fetcher to avoid calling Release in AbortInstall while any of these
  // operations are in progress.
  webstore_data_fetcher_.reset();
}

void WebstoreStandaloneInstaller::CleanUp() {
  // Once install has either completed or aborted, don't observe the
  // Profile lifetime any longer.
  observation_.Reset();
  // Matches the AddRef in BeginInstall.
  Release();
}

}  // namespace extensions
