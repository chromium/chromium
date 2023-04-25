// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_interstitial.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/web_content_handler.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

namespace {

// TODO(b/250924204): Implement shared logic to get the user's given name.
std::u16string GetActiveUserFirstName() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return user_manager::UserManager::Get()->GetActiveUser()->GetGivenName();
#else
  // TODO(b/243656773): Implement for LaCrOS.
  return std::u16string();
#endif
}
}  // namespace

// static
std::unique_ptr<SupervisedUserInterstitial> SupervisedUserInterstitial::Create(
    std::unique_ptr<supervised_user::WebContentHandler> web_content_handler,
    SupervisedUserService& supervised_user_service,
    const GURL& url,
    supervised_user::FilteringBehaviorReason reason) {
  std::unique_ptr<SupervisedUserInterstitial> interstitial = base::WrapUnique(
      new SupervisedUserInterstitial(std::move(web_content_handler),
                                     supervised_user_service, url, reason));

  interstitial->web_content_handler()->CleanUpInfoBarOnMainFrame();
  // Caller is responsible for deleting the interstitial.
  return interstitial;
}

SupervisedUserInterstitial::SupervisedUserInterstitial(
    std::unique_ptr<supervised_user::WebContentHandler> web_content_handler,
    SupervisedUserService& supervised_user_service,
    const GURL& url,
    supervised_user::FilteringBehaviorReason reason)
    : supervised_user_service_(supervised_user_service),
      web_content_handler_(std::move(web_content_handler)),
      url_(url),
      reason_(reason) {}
SupervisedUserInterstitial::~SupervisedUserInterstitial() {}

// static
std::string SupervisedUserInterstitial::GetHTMLContents(
    SupervisedUserService* supervised_user_service,
    PrefService* pref_service,
    supervised_user::FilteringBehaviorReason reason,
    bool already_sent_request,
    bool is_main_frame) {
  std::string custodian = supervised_user_service->GetCustodianName();
  std::string second_custodian =
      supervised_user_service->GetSecondCustodianName();
  std::string custodian_email =
      supervised_user_service->GetCustodianEmailAddress();
  std::string second_custodian_email =
      supervised_user_service->GetSecondCustodianEmailAddress();
  std::string profile_image_url =
      pref_service->GetString(prefs::kSupervisedUserCustodianProfileImageURL);
  std::string profile_image_url2 = pref_service->GetString(
      prefs::kSupervisedUserSecondCustodianProfileImageURL);

  bool allow_access_requests =
      supervised_user_service->remote_web_approvals_manager()
          .AreApprovalRequestsEnabled();

  return supervised_user::BuildErrorPageHtml(
      allow_access_requests, profile_image_url, profile_image_url2, custodian,
      custodian_email, second_custodian, second_custodian_email, reason,
      g_browser_process->GetApplicationLocale(), already_sent_request,
      is_main_frame);
}

void SupervisedUserInterstitial::GoBack() {
  web_content_handler_->GoBack();
  UMA_HISTOGRAM_ENUMERATION(kInterstitialCommandHistogramName, Commands::BACK,
                            Commands::HISTOGRAM_BOUNDING_VALUE);
}

void SupervisedUserInterstitial::RequestUrlAccessRemote(
    base::OnceCallback<void(bool)> callback) {
  UMA_HISTOGRAM_ENUMERATION(kInterstitialCommandHistogramName,
                            Commands::REMOTE_ACCESS_REQUEST,
                            Commands::HISTOGRAM_BOUNDING_VALUE);
  OutputRequestPermissionSourceMetric();

  supervised_user_service_->remote_web_approvals_manager().RequestApproval(
      url_, std::move(callback));
}

void SupervisedUserInterstitial::RequestUrlAccessLocal(
    base::OnceCallback<void(bool)> callback) {
  UMA_HISTOGRAM_ENUMERATION(kInterstitialCommandHistogramName,
                            Commands::LOCAL_ACCESS_REQUEST,
                            Commands::HISTOGRAM_BOUNDING_VALUE);
  OutputRequestPermissionSourceMetric();

  web_content_handler_->RequestLocalApproval(url_, GetActiveUserFirstName(),
                                             std::move(callback));
}

void SupervisedUserInterstitial::ShowFeedback() {
  std::string second_custodian =
      supervised_user_service_->GetSecondCustodianName();

  std::u16string reason = l10n_util::GetStringUTF16(
      supervised_user::GetBlockMessageID(reason_, second_custodian.empty()));
  web_content_handler_->ShowFeedback(url_, reason);
  return;
}

void SupervisedUserInterstitial::OutputRequestPermissionSourceMetric() {
  RequestPermissionSource source;
  if (web_content_handler_->IsMainFrame()) {
    source = RequestPermissionSource::MAIN_FRAME;
  } else {
    source = RequestPermissionSource::SUB_FRAME;
  }

  UMA_HISTOGRAM_ENUMERATION(kInterstitialPermissionSourceHistogramName, source,
                            RequestPermissionSource::HISTOGRAM_BOUNDING_VALUE);
}
