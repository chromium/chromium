// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/javascript_frameworks_ukm_observer.h"

#include "base/feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/mojom/loader/javascript_framework_detection.mojom-forward.h"

JavascriptFrameworksUkmObserver::JavascriptFrameworksUkmObserver() = default;

JavascriptFrameworksUkmObserver::~JavascriptFrameworksUkmObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
JavascriptFrameworksUkmObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // OnLoadingBehaviorObserved events for detecting JavaScript frameworks are
  // only kicked for outermost frames. See DetectJavascriptFrameworksOnLoad in
  // third_party/blink/renderer/core/script/detect_javascript_frameworks.cc
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
JavascriptFrameworksUkmObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Metrics should be collected for Prerendered frames but only recorded after
  // the page has been displayed.
  is_in_prerendered_page_ = true;
  return CONTINUE_OBSERVING;
}

void JavascriptFrameworksUkmObserver::OnJavaScriptFrameworksObserved(
    content::RenderFrameHost* rfh,
    const blink::JavaScriptFrameworkDetectionResult& result) {
  framework_detection_result_ = result;
}

void JavascriptFrameworksUkmObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming&) {
  if (is_in_prerendered_page_)
    return;

  RecordJavascriptFrameworkPageLoad();
}

JavascriptFrameworksUkmObserver::ObservePolicy
JavascriptFrameworksUkmObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming&) {
  if (is_in_prerendered_page_)
    return CONTINUE_OBSERVING;

  RecordJavascriptFrameworkPageLoad();
  return STOP_OBSERVING;
}

void JavascriptFrameworksUkmObserver::RecordJavascriptFrameworkPageLoad() {
  ukm::builders::JavascriptFrameworkPageLoad detect_framework_builder(
      GetDelegate().GetPageUkmSourceId());

  ukm::builders::ContentManagementSystemPageLoad detect_cms_builder(
      GetDelegate().GetPageUkmSourceId());

  using blink::mojom::JavaScriptFramework;

  auto is_detected = [&](JavaScriptFramework framework) -> bool {
    return framework_detection_result_.detected_versions.contains(framework);
  };

  detect_framework_builder
      .SetAngularPageLoad(is_detected(JavaScriptFramework::kAngular))
      .SetGatsbyPageLoad(is_detected(JavaScriptFramework::kGatsby))
      .SetNextJSPageLoad(is_detected(JavaScriptFramework::kNext))
      .SetNuxtJSPageLoad(is_detected(JavaScriptFramework::kNuxt))
      .SetPreactPageLoad(is_detected(JavaScriptFramework::kPreact))
      .SetReactPageLoad(is_detected(JavaScriptFramework::kReact))
      .SetSapperPageLoad(is_detected(JavaScriptFramework::kSapper))
      .SetSveltePageLoad(is_detected(JavaScriptFramework::kSvelte))
      .SetVuePageLoad(is_detected(JavaScriptFramework::kVue))
      .SetVuePressPageLoad(is_detected(JavaScriptFramework::kVuePress));

  detect_framework_builder.Record(ukm::UkmRecorder::Get());

  detect_cms_builder
      .SetDrupalPageLoad(is_detected(JavaScriptFramework::kDrupal))
      .SetJoomlaPageLoad(is_detected(JavaScriptFramework::kJoomla))
      .SetShopifyPageLoad(is_detected(JavaScriptFramework::kShopify))
      .SetSquarespacePageLoad(is_detected(JavaScriptFramework::kSquarespace))
      .SetWixPageLoad(is_detected(JavaScriptFramework::kWix))
      .SetWordPressPageLoad(is_detected(JavaScriptFramework::kWordPress));

  detect_cms_builder.Record(ukm::UkmRecorder::Get());

  ukm::builders::Blink_JavaScriptFramework_Versions versions_builder_jsf(
      GetDelegate().GetPageUkmSourceId());

  typedef ukm::builders::Blink_JavaScriptFramework_Versions& (
      ukm::builders::Blink_JavaScriptFramework_Versions::*
          JavaScriptFrameworkValueSetter)(int64_t);

  ukm::builders::Blink_ContentManagementSystem_Versions versions_builder_cms(
      GetDelegate().GetPageUkmSourceId());

  typedef ukm::builders::Blink_ContentManagementSystem_Versions& (
      ukm::builders::Blink_ContentManagementSystem_Versions::*
          ContentManagementSystemValueSetter)(int64_t);

  auto detect_jsf_version = [&](JavaScriptFramework framework,
                                JavaScriptFrameworkValueSetter setter) {
    auto version =
        framework_detection_result_.detected_versions.find(framework);
    if (version == framework_detection_result_.detected_versions.end() ||
        version->second == blink::kNoFrameworkVersionDetected) {
      return false;
    }

    (versions_builder_jsf.*setter)(version->second);
    return true;
  };

  auto detect_cms_version = [&](JavaScriptFramework framework,
                                ContentManagementSystemValueSetter setter) {
    auto version =
        framework_detection_result_.detected_versions.find(framework);
    if (version == framework_detection_result_.detected_versions.end() ||
        version->second == blink::kNoFrameworkVersionDetected) {
      return false;
    }

    (versions_builder_cms.*setter)(version->second);
    return true;
  };

  if (detect_jsf_version(JavaScriptFramework::kAngular,
                         &ukm::builders::Blink_JavaScriptFramework_Versions::
                             SetAngularVersion) ||
      detect_jsf_version(JavaScriptFramework::kNext,
                         &ukm::builders::Blink_JavaScriptFramework_Versions::
                             SetNextJSVersion) ||
      detect_jsf_version(
          JavaScriptFramework::kNuxt,
          &ukm::builders::Blink_JavaScriptFramework_Versions::SetNuxtVersion) ||
      detect_jsf_version(
          JavaScriptFramework::kVue,
          &ukm::builders::Blink_JavaScriptFramework_Versions::SetVueVersion)) {
    versions_builder_jsf.Record(ukm::UkmRecorder::Get());
  }

  if (detect_cms_version(
          JavaScriptFramework::kDrupal,
          &ukm::builders::Blink_ContentManagementSystem_Versions::
              SetDrupalVersion) ||
      detect_cms_version(
          JavaScriptFramework::kWordPress,
          &ukm::builders::Blink_ContentManagementSystem_Versions::
              SetWordPressVersion)) {
    versions_builder_cms.Record(ukm::UkmRecorder::Get());
  }
}

void JavascriptFrameworksUkmObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  DCHECK(is_in_prerendered_page_);
  is_in_prerendered_page_ = false;
}
