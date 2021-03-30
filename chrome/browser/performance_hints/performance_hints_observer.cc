// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_hints/performance_hints_observer.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/performance_hints/performance_hints_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/url_pattern_with_wildcards.h"
#include "components/optimization_guide/proto/performance_hints_metadata.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "chrome/browser/performance_hints/android/jni_headers/PerformanceHintsObserver_jni.h"
#include "url/android/gurl_android.h"
#endif  // OS_ANDROID

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::URLPatternWithWildcards;
using optimization_guide::proto::LinkPerformanceMetadata;
using optimization_guide::proto::PerformanceClass;
using optimization_guide::proto::PerformanceHint;

namespace performance_hints {
namespace {

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with:
//  - "PerformanceHintsPerformanceClass" in
//    src/tools/metrics/histograms/enums.xml
//  - "PerformanceClass" in
//    src/components/optimization_guide/proto/performance_hints_metadata.proto
enum class UmaPerformanceClass {
  kUnknown = 0,
  kSlow = 1,
  kFast = 2,
  kNormal = 3,
  kMaxValue = kNormal,
};

UmaPerformanceClass ToUmaPerformanceClass(PerformanceClass performance_class) {
  if (static_cast<int>(performance_class) < 0) {
    NOTREACHED();
    return UmaPerformanceClass::kUnknown;
  } else if (static_cast<int>(performance_class) >
             static_cast<int>(UmaPerformanceClass::kMaxValue)) {
    NOTREACHED();
    return UmaPerformanceClass::kUnknown;
  } else {
    return static_cast<UmaPerformanceClass>(performance_class);
  }
}

// New values should be added to the PerformanceHintsSource histogram_suffix.
enum class HintLookupSource {
  kLinkHint = 0,
  kPageHint = 1,
  kFastHostHint = 2,
  kMaxValue = kFastHostHint,
};

const char* ToString(HintLookupSource source) {
  switch (source) {
    case HintLookupSource::kLinkHint:
      return "LinkHint";
    case HintLookupSource::kPageHint:
      return "PageHint";
    case HintLookupSource::kFastHostHint:
      return "FastHostHint";
  }
}
}  // namespace

#if defined(OS_ANDROID)
static jint JNI_PerformanceHintsObserver_GetPerformanceClassForURL(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents,
    const base::android::JavaParamRef<jobject>& url) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  return PerformanceHintsObserver::PerformanceClassForURL(
      web_contents, *url::GURLAndroid::ToNativeGURL(env, url),
      /*record_metrics=*/false);
}

static jboolean
JNI_PerformanceHintsObserver_IsContextMenuPerformanceInfoEnabled(JNIEnv* env) {
  return features::IsContextMenuPerformanceInfoEnabled();
}
#endif  // OS_ANDROID

PerformanceHintsObserver::PerformanceHintsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // Given that we throw things away if the user is unable to fetch from
  // the remote Optimization Guide, do not instantiate anything else since
  // we will throw everything away anyway. Although FAST_HOST_HINTS and some
  // hints that were fetched for the original profile are available in
  // incognito, these do not provide sufficient coverage.
  if (profile->IsOffTheRecord())
    return;

  optimization_guide_decider_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  std::vector<optimization_guide::proto::OptimizationType> opts;
  opts.push_back(optimization_guide::proto::PERFORMANCE_HINTS);
  if (features::AreFastHostHintsEnabled()) {
    opts.push_back(optimization_guide::proto::FAST_HOST_HINTS);
  }
  if (features::AreLinkPerformanceHintsEnabled()) {
    opts.push_back(optimization_guide::proto::LINK_PERFORMANCE);
  }
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(opts);
  }

  rewrite_handler_ =
      RewriteHandler::FromConfigString(features::GetRewriteConfigString());
}

PerformanceHintsObserver::~PerformanceHintsObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
PerformanceClass PerformanceHintsObserver::PerformanceClassForURL(
    content::WebContents* web_contents,
    const GURL& url,
    bool record_metrics) {
  if (web_contents == nullptr) {
    return PerformanceClass::PERFORMANCE_UNKNOWN;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile ||
      !optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile->IsOffTheRecord(), profile->GetPrefs())) {
    // We can't get performance hints if OptimizationGuide can't fetch them.
    return PerformanceClass::PERFORMANCE_UNKNOWN;
  }

  PerformanceHintsObserver* performance_hints_observer =
      PerformanceHintsObserver::FromWebContents(web_contents);
  if (performance_hints_observer == nullptr ||
      !performance_hints_observer->DoesPageSupportHints()) {
    return PerformanceClass::PERFORMANCE_UNKNOWN;
  }

  HintForURLResult result =
      performance_hints_observer->HintForURL(url, record_metrics);
  if (record_metrics) {
    if (result.rewritten) {
      UMA_HISTOGRAM_ENUMERATION(
          "PerformanceHints.Observer.HintForURLResult.Rewritten",
          result.status);
    }
    UMA_HISTOGRAM_ENUMERATION("PerformanceHints.Observer.HintForURLResult",
                              result.status);
  }

  PerformanceClass performance_class;
  switch (result.status) {
    case HintForURLStatus::kHintFound:
      performance_class = result.hint ? result.hint->performance_class()
                                      : PerformanceClass::PERFORMANCE_UNKNOWN;
      break;
    case HintForURLStatus::kHintNotFound:
    case HintForURLStatus::kHintNotReady:
      performance_class = PerformanceClass::PERFORMANCE_UNKNOWN;
      break;
    case HintForURLStatus::kInvalidURL:
      // Error case. Don't allow the override.
      return PerformanceClass::PERFORMANCE_UNKNOWN;
  }

  if (record_metrics) {
    // Log to UMA before the override logic so we can determine how often the
    // override is happening.
    UMA_HISTOGRAM_ENUMERATION(
        "PerformanceHints.Observer.PerformanceClassForURL",
        ToUmaPerformanceClass(performance_class));
  }

  if (performance_class == PerformanceClass::PERFORMANCE_UNKNOWN &&
      features::ShouldTreatUnknownAsFast()) {
    // If we couldn't get the hint or we didn't expect it on this page, give it
    // the benefit of the doubt.
    return PerformanceClass::PERFORMANCE_FAST;
  }

  return performance_class;
}

// static
void PerformanceHintsObserver::RecordPerformanceUMAForURL(
    content::WebContents* web_contents,
    const GURL& url) {
  PerformanceClassForURL(web_contents, url, /*record_metrics=*/true);
}

PerformanceHintsObserver::HintForURLResult::HintForURLResult() = default;
PerformanceHintsObserver::HintForURLResult::HintForURLResult(
    const HintForURLResult&) = default;
PerformanceHintsObserver::HintForURLResult::~HintForURLResult() = default;

PerformanceHintsObserver::HintForURLResult PerformanceHintsObserver::HintForURL(
    const GURL& url,
    bool record_metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HintForURLResult result;

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    result.status = HintForURLStatus::kInvalidURL;
    return result;
  }

  base::Optional<GURL> maybe_rewritten;
  if (features::ShouldHandleRewrites()) {
    maybe_rewritten = rewrite_handler_.HandleRewriteIfNecessary(url);
    result.rewritten = maybe_rewritten.has_value();
    if (maybe_rewritten && (!maybe_rewritten->is_valid() ||
                            !maybe_rewritten->SchemeIsHTTPOrHTTPS())) {
      result.status = HintForURLStatus::kInvalidURL;
      return result;
    }
  }

  GURL hint_url = maybe_rewritten.value_or(url);

  // At this point we know the URL is valid. Individual source lookups will
  // override this if they have more detailed status (found or not ready).
  result.status = HintForURLStatus::kHintNotFound;

  using LookupFn = base::OnceCallback<std::tuple<
      SourceLookupStatus,
      base::Optional<optimization_guide::proto::PerformanceHint>>(const GURL&)>;

  std::vector<std::tuple<HintLookupSource, LookupFn>> sources;
  sources.emplace_back(HintLookupSource::kLinkHint,
                       base::BindOnce(&PerformanceHintsObserver::LinkHintForURL,
                                      base::Unretained(this)));
  sources.emplace_back(HintLookupSource::kPageHint,
                       base::BindOnce(&PerformanceHintsObserver::PageHintForURL,
                                      base::Unretained(this)));
  if (features::AreFastHostHintsEnabled()) {
    sources.emplace_back(
        HintLookupSource::kFastHostHint,
        base::BindOnce(&PerformanceHintsObserver::FastHostHintForURL,
                       base::Unretained(this)));
  }

  for (std::tuple<HintLookupSource, LookupFn>& source : sources) {
    SourceLookupStatus lookup_status = SourceLookupStatus::kNotQueried;
    // Only query sources until a hint has been found.
    if (!result.hint.has_value()) {
      std::tie(lookup_status, result.hint) =
          std::move(std::get<LookupFn>(source)).Run(hint_url);
    }
    if (record_metrics) {
      // UMA is recorded for each source, even if it wasn't queried. This is
      // done so all source histograms have the same total.
      base::UmaHistogramEnumeration(
          base::StrCat({"PerformanceHints.Observer.SourceLookupStatus.",
                        ToString(std::get<HintLookupSource>(source))}),
          lookup_status);
    }
    switch (lookup_status) {
      case SourceLookupStatus::kNotReady:
        // If no hints are found and any of the sources returned kNotReady, we
        // should also return kNotReady.
        result.status = HintForURLStatus::kHintNotReady;
        break;
      case SourceLookupStatus::kHintFound:
        DCHECK(result.hint.has_value());
        result.status = HintForURLStatus::kHintFound;
        break;
      case SourceLookupStatus::kNotQueried:
      case SourceLookupStatus::kNoMatch:
        break;
    }
  }

  return result;
}

bool PerformanceHintsObserver::DoesPageSupportHints() {
  // page_url_ is not set for error pages.
  return page_url_.has_value() && page_url_->is_valid() &&
         page_url_->SchemeIsHTTPOrHTTPS();
}

void PerformanceHintsObserver::PopulateLinkHints() {
  DCHECK(page_url_);
  if (!page_url_)
    return;

  const google::protobuf::RepeatedPtrField<PerformanceHint>* link_hints =
      nullptr;
  // Metadata variables are scoped here to share the same scope as link_hints.
  optimization_guide::OptimizationMetadata metadata;
  base::Optional<LinkPerformanceMetadata> link_metadata;
  if (features::AreLinkPerformanceHintsEnabled()) {
    link_hints_decision_ = optimization_guide_decider_->CanApplyOptimization(
        page_url_.value(), optimization_guide::proto::LINK_PERFORMANCE,
        &metadata);
    link_metadata = metadata.ParsedMetadata<LinkPerformanceMetadata>();
    if (!link_metadata)
      return;
    link_hints = &link_metadata->link_hints();
  } else {
    link_hints_decision_ = optimization_guide_decider_->CanApplyOptimization(
        page_url_.value(), optimization_guide::proto::PERFORMANCE_HINTS,
        &metadata);
    if (!metadata.performance_hints_metadata())
      return;
    link_hints =
        &metadata.performance_hints_metadata().value().performance_hints();
  }

  DCHECK(link_hints);
  if (link_hints_decision_ == OptimizationGuideDecision::kTrue) {
    for (const PerformanceHint& link_hint : *link_hints) {
      link_hints_.emplace_back(
          URLPatternWithWildcards(link_hint.wildcard_pattern()), link_hint);
    }
  }
}

std::tuple<PerformanceHintsObserver::SourceLookupStatus,
           base::Optional<optimization_guide::proto::PerformanceHint>>
PerformanceHintsObserver::LinkHintForURL(const GURL& url) {
  if (!optimization_guide_decider_) {
    return {SourceLookupStatus::kNoMatch, base::nullopt};
  }

  if (link_hints_decision_ == OptimizationGuideDecision::kUnknown) {
    PopulateLinkHints();
  }
  switch (link_hints_decision_) {
    case OptimizationGuideDecision::kUnknown:
      return {SourceLookupStatus::kNotReady, base::nullopt};
    case OptimizationGuideDecision::kFalse:
      return {SourceLookupStatus::kNoMatch, base::nullopt};
    case OptimizationGuideDecision::kTrue: {
      // Link hints only contain scheme, host, and path, so remove other
      // components.
      url::Replacements<char> replacements;
      replacements.ClearUsername();
      replacements.ClearPassword();
      replacements.ClearQuery();
      replacements.ClearPort();
      replacements.ClearRef();
      GURL scheme_host_path = url.ReplaceComponents(replacements);

      for (const auto& pattern_hint : link_hints_) {
        if (pattern_hint.first.Matches(scheme_host_path.spec())) {
          return {SourceLookupStatus::kHintFound, pattern_hint.second};
        }
      }
      return {SourceLookupStatus::kNoMatch, base::nullopt};
    }
  }
}

std::tuple<PerformanceHintsObserver::SourceLookupStatus,
           base::Optional<optimization_guide::proto::PerformanceHint>>
PerformanceHintsObserver::PageHintForURL(const GURL& url) const {
  if (!optimization_guide_decider_) {
    return {SourceLookupStatus::kNoMatch, base::nullopt};
  }

  // Check to see if there happens to be a cached hint for the site that this
  // URL belongs to. This should be the case for links on the SRP since the
  // OptimizationGuideService proactively fetches hints for them.
  optimization_guide::OptimizationMetadata metadata;
  OptimizationGuideDecision decision =
      optimization_guide_decider_->CanApplyOptimization(
          url, optimization_guide::proto::PERFORMANCE_HINTS, &metadata);
  if (decision == OptimizationGuideDecision::kUnknown) {
    return {SourceLookupStatus::kNotReady, base::nullopt};
  } else if (decision == OptimizationGuideDecision::kTrue &&
             metadata.performance_hints_metadata() &&
             metadata.performance_hints_metadata()->has_page_hint()) {
    return {SourceLookupStatus::kHintFound,
            metadata.performance_hints_metadata()->page_hint()};
  }

  return {SourceLookupStatus::kNoMatch, base::nullopt};
}

std::tuple<PerformanceHintsObserver::SourceLookupStatus,
           base::Optional<optimization_guide::proto::PerformanceHint>>
PerformanceHintsObserver::FastHostHintForURL(const GURL& url) const {
  if (!optimization_guide_decider_) {
    return {SourceLookupStatus::kNoMatch, base::nullopt};
  }

  OptimizationGuideDecision decision =
      optimization_guide_decider_->CanApplyOptimization(
          url, optimization_guide::proto::FAST_HOST_HINTS, nullptr);
  switch (decision) {
    case OptimizationGuideDecision::kTrue: {
      optimization_guide::proto::PerformanceHint hint;
      hint.set_performance_class(optimization_guide::proto::PERFORMANCE_FAST);
      return {SourceLookupStatus::kHintFound, hint};
    }
    case OptimizationGuideDecision::kFalse:
      return {SourceLookupStatus::kNoMatch, base::nullopt};
    case OptimizationGuideDecision::kUnknown:
      return {SourceLookupStatus::kNotReady, base::nullopt};
  }
}

void PerformanceHintsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    // Use the same hints if the main frame hasn't changed.
    return;
  }

  // We've navigated to a new page, so clear out any existing cached hints.
  link_hints_.clear();
  link_hints_decision_ = OptimizationGuideDecision::kUnknown;
  page_url_.reset();

  if (!optimization_guide_decider_) {
    return;
  }
  if (navigation_handle->IsErrorPage()) {
    // Don't provide hints on Chrome error pages.
    return;
  }

  page_url_ = navigation_handle->GetURL();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PerformanceHintsObserver)

}  // namespace performance_hints
