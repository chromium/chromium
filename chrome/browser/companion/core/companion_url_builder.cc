// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_url_builder.h"

#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/companion/core/companion_permission_utils.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/proto/companion_url_params.pb.h"
#include "chrome/browser/companion/core/signin_delegate.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace companion {
namespace {

// URL query string param name that contains the request params for companion
// page in protobuf format.
inline constexpr char kCompanionRequestQueryParameterKey[] = "companion_query";

// Query parameter for the url of the main web content.
inline constexpr char kUrlQueryParameterKey[] = "url";
// Query parameter for the Chrome WebUI origin.
inline constexpr char kOriginQueryParameterKey[] = "origin";
// Query parameter for the search text query.
inline constexpr char kTextQueryParameterKey[] = "q";
// Query parameter value for the Chrome WebUI origin. This needs to be different
// from the WebUI URL constant because it does not include the last '/'.
inline constexpr char kOriginQueryParameterValue[] =
    "chrome-untrusted://companion-side-panel.top-chrome";

}  // namespace

CompanionUrlBuilder::CompanionUrlBuilder(PrefService* pref_service,
                                         SigninDelegate* signin_delegate)
    : pref_service_(pref_service), signin_delegate_(signin_delegate) {}

CompanionUrlBuilder::~CompanionUrlBuilder() = default;

GURL CompanionUrlBuilder::BuildCompanionURL(const GURL& page_url) {
  return BuildCompanionURL(page_url, /*text_query=*/"");
}

GURL CompanionUrlBuilder::BuildCompanionURL(
    const GURL& page_url,
    const std::string& text_query,
    std::unique_ptr<base::Time> text_query_start_time) {
  return AppendCompanionParamsToURL(GURL(GetHomepageURLForCompanion()),
                                    page_url, text_query,
                                    std::move(text_query_start_time));
}

GURL CompanionUrlBuilder::AppendCompanionParamsToURL(
    const GURL& base_url,
    const GURL& page_url,
    const std::string& text_query,
    std::unique_ptr<base::Time> text_query_start_time) {
  GURL url_with_query_params = base_url;
  // Fill the protobuf with the required query params.
  std::string base64_encoded_proto =
      BuildCompanionUrlParamProto(page_url, std::move(text_query_start_time));
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kCompanionRequestQueryParameterKey,
      base64_encoded_proto);

  // Add origin as a param allowing the page to be iframed.
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kOriginQueryParameterKey,
      kOriginQueryParameterValue);

  // TODO(b/274714162): Remove URL param.
  if (IsUserPermittedToSharePageURLWithCompanion(pref_service_) &&
      IsValidPageURLForCompanion(page_url)) {
    url_with_query_params = net::AppendOrReplaceQueryParameter(
        url_with_query_params, kUrlQueryParameterKey, page_url.spec());
  }

  if (!text_query.empty()) {
    url_with_query_params = net::AppendOrReplaceQueryParameter(
        url_with_query_params, kTextQueryParameterKey, text_query);
  }
  return url_with_query_params;
}

std::string CompanionUrlBuilder::BuildCompanionUrlParamProto(
    const GURL& page_url,
    std::unique_ptr<base::Time> text_query_start_time) {
  // Fill the protobuf with the required query params.
  companion::proto::CompanionUrlParams url_params;
  if (IsUserPermittedToSharePageURLWithCompanion(pref_service_) &&
      IsValidPageURLForCompanion(page_url)) {
    url_params.set_page_url(page_url.spec());
  }

  url_params.set_is_page_url_sharing_enabled(
      IsUserPermittedToSharePageURLWithCompanion(pref_service_));
  url_params.set_is_page_content_sharing_enabled(
      IsUserPermittedToSharePageContentWithCompanion(pref_service_));
  url_params.set_is_page_content_enabled(
      base::FeatureList::IsEnabled(features::kCompanionEnablePageContent));
  url_params.set_is_sign_in_allowed(signin_delegate_->AllowedSignin());
  url_params.set_is_signed_in(signin_delegate_->IsSignedIn());
  url_params.set_links_open_in_new_tab(
      !companion::ShouldOpenLinksInCurrentTab());
  if (text_query_start_time) {
    // Add the query start time to the parameters if present.
    companion::proto::Timestamp* query_start_time =
        url_params.mutable_query_start_time();
    int64_t nanoseconds_in_milliseconds = 1e6;
    int64_t time_nanoseconds =
        text_query_start_time->InMillisecondsSinceUnixEpoch() *
        nanoseconds_in_milliseconds;
    query_start_time->set_seconds(time_nanoseconds /
                                  base::Time::kNanosecondsPerSecond);
    query_start_time->set_nanos(time_nanoseconds %
                                base::Time::kNanosecondsPerSecond);
  }

// Need to BUILDFLAG these lines because kSidePanelCompanionEntryPinnedToToolbar
// and kVisualQuerySuggestions do not exist on Android and will break try-bots
#if (!BUILDFLAG(IS_ANDROID))
  bool is_entry_point_default_pinned =
      pref_service_ &&
      pref_service_
          ->GetDefaultPrefValue(prefs::kSidePanelCompanionEntryPinnedToToolbar)
          ->GetBool();
  url_params.set_is_entrypoint_pinned_by_default(is_entry_point_default_pinned);
  url_params.set_is_vqs_enabled_on_chrome(false);
  url_params.set_is_upload_dialog_supported(true);
  url_params.set_is_hard_refresh_supported(true);
#endif

  companion::proto::PromoState* promo_state = url_params.mutable_promo_state();
  promo_state->set_signin_promo_denial_count(
      pref_service_->GetInteger(kSigninPromoDeclinedCountPref));
  promo_state->set_msbb_promo_denial_count(
      pref_service_->GetInteger(kMsbbPromoDeclinedCountPref));
  promo_state->set_exps_promo_denial_count(
      pref_service_->GetInteger(kExpsPromoDeclinedCountPref));
  promo_state->set_exps_promo_shown_count(
      pref_service_->GetInteger(kExpsPromoShownCountPref));
  promo_state->set_pco_promo_shown_count(
      pref_service_->GetInteger(kPcoPromoShownCountPref));
  promo_state->set_pco_promo_denial_count(
      pref_service_->GetInteger(kPcoPromoDeclinedCountPref));

  // Set region search IPH state.
  promo_state->set_should_show_region_search_iph(
      signin_delegate_->ShouldShowRegionSearchIPH());

  std::string base64_encoded_proto;
  base::Base64UrlEncode(url_params.SerializeAsString(),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &base64_encoded_proto);
  return base64_encoded_proto;
}

}  // namespace companion
