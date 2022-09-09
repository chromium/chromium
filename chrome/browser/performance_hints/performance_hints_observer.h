// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_HINTS_PERFORMANCE_HINTS_OBSERVER_H_
#define CHROME_BROWSER_PERFORMANCE_HINTS_PERFORMANCE_HINTS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/performance_hints/rewrite_handler.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {
class URLPatternWithWildcards;
class OptimizationGuideDecider;
namespace proto {
class PerformanceHint;
}  // namespace proto
}  // namespace optimization_guide

namespace performance_hints {

// Provides an interface to access PerformanceHints for the associated
// WebContents and links within it.
class PerformanceHintsObserver
    : public content::WebContentsUserData<PerformanceHintsObserver> {
 public:
  ~PerformanceHintsObserver() override;
  PerformanceHintsObserver(const PerformanceHintsObserver&) = delete;
  PerformanceHintsObserver& operator=(const PerformanceHintsObserver&) = delete;

  // Returns the PerformanceClass for |url|, or PERFORMANCE_UNKNOWN if could not
  // be determined.
  //
  // UMA metrics are only recorded when |record_metrics| is true. Callers should
  // record metrics only once per badging decision, e.g. once per context menu
  // shown. If the same PerformanceClass is needed again, |record_metrics|
  // should be set to false.
  static optimization_guide::proto::PerformanceClass PerformanceClassForURL(
      content::WebContents* web_contents,
      const GURL& url,
      bool record_metrics);

  // Used only on Android since the java API can be called repeatedly and is
  // not appropriate to record metrics.
  static void RecordPerformanceUMAForURL(content::WebContents* web_contents,
                                         const GURL& url);

 private:
  explicit PerformanceHintsObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PerformanceHintsObserver>;
  friend class PerformanceHintsObserverTest;

  // SourceLookupStatus represents the result of a querying a single source
  // (page hints or link hints) for performance information. Tracking this
  // separately from the overall HintForURLStatus (below) allows us to determine
  // individual source coverage and how often each source is ready to respond.
  //
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "PerformanceHintsObserverSourceLookupStatus" in
  // src/tools/metrics/histograms/enums.xml.
  enum class SourceLookupStatus {
    // The source was not queried for a given URL lookup (i.e. a hint was found
    // in a preceding source).
    kNotQueried = 0,
    // The source didn't have an answer in time.
    kNotReady = 1,
    // The source was ready but no matching hint was found.
    kNoMatch = 2,
    // A matching hint was found for the URL.
    kHintFound = 3,
    kMaxValue = kHintFound,
  };

  // Data scoped to a single page. PageData has the same lifetime as the page's
  // main document.
  class PageData : public content::PageUserData<PageData> {
   public:
    explicit PageData(content::Page& page);
    PageData(const PageData&) = delete;
    PageData& operator=(const PageData&) = delete;
    ~PageData() override;

    void SetLinkHintDecision(
        optimization_guide::OptimizationGuideDecision decision) {
      link_hints_decision_ = decision;
    }

    optimization_guide::OptimizationGuideDecision GetLinkHintDecision() const {
      return link_hints_decision_;
    }

    void SetLinkHints(const google::protobuf::RepeatedPtrField<
                      optimization_guide::proto::PerformanceHint>& link_hints);

    // |url| should be stripped to scheme://host/path.
    std::tuple<SourceLookupStatus,
               absl::optional<optimization_guide::proto::PerformanceHint>>
    GetLinkHintForURL(const GURL& url) const;

    PAGE_USER_DATA_KEY_DECL();

   private:
    // Link URLs that match |first| should use the Performance hint in |second|.
    std::vector<std::pair<optimization_guide::URLPatternWithWildcards,
                          optimization_guide::proto::PerformanceHint>>
        link_hints_;

    // The fetch status for link hints.
    optimization_guide::OptimizationGuideDecision link_hints_decision_ =
        optimization_guide::OptimizationGuideDecision::kUnknown;
  };

  // Returns true if the current page supports hints (not an error page,
  // must be HTTP/HTTPS, etc).
  bool DoesPageSupportHints();

  // Populates |link_hints_| in PageData with performance information for links
  // on the current page.
  void PopulateLinkHints(PageData& page_data);

  // Attempts to retrieve a PerformanceHint for |url| from the link hints of the
  // current page.
  std::tuple<SourceLookupStatus,
             absl::optional<optimization_guide::proto::PerformanceHint>>
  LinkHintForURL(const GURL& url);

  // Attempts to retrieve a PerformanceHint for |url| from the OptimizationGuide
  // metadata for that URL.
  std::tuple<SourceLookupStatus,
             absl::optional<optimization_guide::proto::PerformanceHint>>
  PageHintForURL(const GURL& url) const;

  // Attempts to retrieve a PerformanceHint for |url| from the fast host bloom
  // filter.
  std::tuple<SourceLookupStatus,
             absl::optional<optimization_guide::proto::PerformanceHint>>
  FastHostHintForURL(const GURL& url) const;

  // HintForURLStatus represents the overall lookup result for a given URL.
  // Exactly one sample will be recorded for each call to HintForURL.
  //
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "PerformanceHintsObserverHintForURLResult" in
  // src/tools/metrics/histograms/enums.xml.
  enum class HintForURLStatus {
    // Hints for the current page have been processed and no hint for the URL
    // was found.
    kHintNotFound = 0,
    // One or more sources could not answer in time. The call may be attempted
    // again.
    kHintNotReady = 1,
    // An invalid URL was passed.
    kInvalidURL = 2,
    // A matching hint was found and has been returned.
    kHintFound = 3,
    kMaxValue = kHintFound,
  };

  struct HintForURLResult {
    HintForURLResult();
    HintForURLResult(const HintForURLResult&);
    ~HintForURLResult();

    // Describes the fetch outcome. See HintForURLStatus for details.
    HintForURLStatus status = HintForURLStatus::kHintNotFound;
    // True if the URL was rewritten before lookups were done. False otherwise.
    bool rewritten = false;
    // If status == kHintFound, this will contain the matching hint.
    absl::optional<optimization_guide::proto::PerformanceHint> hint =
        absl::nullopt;
  };

  // Fetches a PerformanceHint for the given |url|.
  //
  // See HintForURLResult for details on the return value.
  HintForURLResult HintForURL(const GURL& url, bool record_metrics);

  // If kPerformanceHintsHandleRewrites is enabled, URLs that match one of the
  // configured rewrite patterns will have the inner URL extracted and used for
  // hint matching.
  //
  // Configuration is controlled by kRewriteConfig in
  // performance_hints_observer.cc.
  RewriteHandler rewrite_handler_;

  // Initialized in constructor. It may be null if !IsOptimizationHintsEnabled.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PerformanceHintsObserver> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace performance_hints

#endif  // CHROME_BROWSER_PERFORMANCE_HINTS_PERFORMANCE_HINTS_OBSERVER_H_
