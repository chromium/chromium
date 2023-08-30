// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_url_builder.h"

#include "base/base64.h"
#include "chrome/browser/companion/core/companion_permission_utils.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/proto/companion_url_params.pb.h"
#include "chrome/browser/companion/core/signin_delegate.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

// Need to BUILDFLAG these lines because kSidePanelCompanionEntryPinnedToToolbar
// does not exist on Android and will break try-bots
#if (!BUILDFLAG(IS_ANDROID))
#include "chrome/browser/companion/visual_search/features.h"
#endif

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

GURL CompanionUrlBuilder::BuildCompanionURL(const GURL& page_url,
                                            const std::string& text_query) {
  return AppendCompanionParamsToURL(GURL(GetHomepageURLForCompanion()),
                                    page_url, text_query);
}

GURL CompanionUrlBuilder::AppendCompanionParamsToURL(
    const GURL& base_url,
    const GURL& page_url,
    const std::string& text_query) {
  GURL url_with_query_params = base_url;
  // Fill the protobuf with the required query params.
  std::string base64_encoded_proto = BuildCompanionUrlParamProto(page_url);
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kCompanionRequestQueryParameterKey,
      base64_encoded_proto);

  // Add origin as a param allowing the page to be iframed.
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kOriginQueryParameterKey,
      kOriginQueryParameterValue);

  // TODO(b/274714162): Remove URL param.
  bool is_msbb_enabled =
      IsUserPermittedToSharePageInfoWithCompanion(pref_service_);
  if (is_msbb_enabled && IsValidPageURLForCompanion(page_url)) {
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
    const GURL& page_url) {
  // Fill the protobuf with the required query params.
  companion::proto::CompanionUrlParams url_params;
  bool is_msbb_enabled =
      IsUserPermittedToSharePageInfoWithCompanion(pref_service_);
  if (is_msbb_enabled && IsValidPageURLForCompanion(page_url)) {
    url_params.set_page_url(page_url.spec());
  }

  url_params.set_has_msbb_enabled(is_msbb_enabled);
  url_params.set_is_sign_in_allowed(signin_delegate_->AllowedSignin());
  url_params.set_is_signed_in(signin_delegate_->IsSignedIn());
  url_params.set_links_open_in_new_tab(
      !companion::ShouldOpenLinksInCurrentTab());

// Need to BUILDFLAG these lines because kSidePanelCompanionEntryPinnedToToolbar
// and kVisualSearchSuggestions do not exist on Android and will break try-bots
#if (!BUILDFLAG(IS_ANDROID))
  bool is_entry_point_default_pinned =
      pref_service_ &&
      pref_service_
          ->GetDefaultPrefValue(prefs::kSidePanelCompanionEntryPinnedToToolbar)
          ->GetBool();
  url_params.set_is_entrypoint_pinned_by_default(is_entry_point_default_pinned);
  url_params.set_is_vqs_enabled_on_chrome(base::FeatureList::IsEnabled(
      visual_search::features::kVisualSearchSuggestions));
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

  // Set region search IPH state.
  promo_state->set_should_show_region_search_iph(
      signin_delegate_->ShouldShowRegionSearchIPH());

  std::string base64_encoded_proto;
  base::Base64Encode(url_params.SerializeAsString(), &base64_encoded_proto);
  return base64_encoded_proto;
}

}  // namespace companion
