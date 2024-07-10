// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/formfill_page_load_metrics_observer.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {

const char kUserDataFieldFilledKey[] = "UserDataFieldFilled";

}  // namespace

FormfillPageLoadMetricsObserver::FormfillPageLoadMetricsObserver() = default;

FormfillPageLoadMetricsObserver::~FormfillPageLoadMetricsObserver() = default;

const char* FormfillPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "FormfillPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FormfillPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // OnFeaturesUsageObserved needs observer level forwarding.
  return FORWARD_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FormfillPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FormfillPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!GetDelegate().IsInPrerenderingBeforeActivationStart())
    MaybeRecordPriorUsageOfUserData(navigation_handle);

  return CONTINUE_OBSERVING;
}

void FormfillPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  MaybeRecordPriorUsageOfUserData(navigation_handle);
}

void FormfillPageLoadMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  if (GetDelegate().IsInPrerenderingBeforeActivationStart())
    return;

  if (user_data_field_detected_)
    return;

  bool observed_user_data_field_detected_feature = base::ranges::any_of(
      features, [](const blink::UseCounterFeature& feature) {
        return feature.type() ==
                   blink::mojom::UseCounterFeatureType::kWebFeature &&
               (static_cast<blink::mojom::WebFeature>(feature.value()) ==
                    blink::mojom::WebFeature::
                        kUserDataFieldFilled_PredictedTypeMatch ||
                static_cast<blink::mojom::WebFeature>(feature.value()) ==
                    blink::mojom::WebFeature::kEmailFieldFilled_PatternMatch);
      });

  if (!observed_user_data_field_detected_feature)
    return;

  user_data_field_detected_ = true;

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(Profile::FromBrowserContext(
          GetDelegate().GetWebContents()->GetBrowserContext()));
  DCHECK(settings_map);

  const GURL& url = rfh->GetLastCommittedURL();
  base::Value formfill_metadata = settings_map->GetWebsiteSetting(
      url, url, ContentSettingsType::FORMFILL_METADATA, nullptr);

  if (!formfill_metadata.is_dict()) {
    formfill_metadata = base::Value(base::Value::Type::DICT);
  }

  if (!formfill_metadata.GetDict().FindBool(kUserDataFieldFilledKey)) {
    formfill_metadata.GetDict().Set(kUserDataFieldFilledKey, true);

    settings_map->SetWebsiteSettingDefaultScope(
        url, url, ContentSettingsType::FORMFILL_METADATA,
        std::move(formfill_metadata));
  }
}

// Check if |kUserDataFieldFilledKey| has been previously set for the associated
// URL.
void FormfillPageLoadMetricsObserver::MaybeRecordPriorUsageOfUserData(
    content::NavigationHandle* navigation_handle) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(Profile::FromBrowserContext(
          GetDelegate().GetWebContents()->GetBrowserContext()));
  DCHECK(settings_map);

  const GURL& url =
      navigation_handle->GetRenderFrameHost()->GetLastCommittedURL();

  base::Value formfill_metadata = settings_map->GetWebsiteSetting(
      url, url, ContentSettingsType::FORMFILL_METADATA, nullptr);

  // User data field was detected on this site before.
  if (formfill_metadata.is_dict() &&
      formfill_metadata.GetDict().FindBool(kUserDataFieldFilledKey)) {
    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        navigation_handle->GetRenderFrameHost(),
        blink::mojom::WebFeature::kUserDataFieldFilledPreviously);
  }
}
