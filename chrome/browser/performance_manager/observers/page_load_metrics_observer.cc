// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/buildflags/buildflags.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_manager.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#else
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

namespace performance_manager {

namespace {

enum class WebContentsType {
  kTab,
  kPrerender,
  kExtension,
  kDevTools,
  kUnknown,
};

// Types of navigations that can occur during a "pageload". If multiple
// navigations occur during the same "pageload", the lowest value is used to
// determine the type of the "pageload". "Different-document" navigations are
// first because they consume more resources than "same-document" navigations,
// and we want to be able to identify resource-consuming "pageloads". Values in
// this enum are used as offset from *Base values in the LoadType enum below.
enum class NavigationType {
  kMainFrameDifferentDocument = 0,
  kSubFrameDifferentDocument = 1,
  kMainFrameSameDocument = 2,
  kSubFrameSameDocument = 3,
  kNoCommit = 4,
  kCount,
};

// Bucketize |load_count| using an exponential function to minimize bits of data
// sent through UKM. The bucket spacing is chosen to have exact counts until 20.
// go/exponential-bucketing-for-ukm-discussion
int64_t BucketizeLoadCount(int load_count) {
  constexpr double kBucketSpacing = 1.1;
  return ukm::GetExponentialBucketMin(load_count, kBucketSpacing);
}

// Listens to content::WebContentsObserver notifications and records metrics
// for a given WebContents.
class PageLoadMetricsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PageLoadMetricsWebContentsObserver> {
 public:
  explicit PageLoadMetricsWebContentsObserver(
      content::WebContents* web_contents);

  PageLoadMetricsWebContentsObserver(
      const PageLoadMetricsWebContentsObserver&) = delete;
  PageLoadMetricsWebContentsObserver& operator=(
      const PageLoadMetricsWebContentsObserver&) = delete;

  ~PageLoadMetricsWebContentsObserver() override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  WebContentsType GetWebContentsType();

  bool IsTab() const;
  bool IsExtension() const;
  bool IsDevTools() const;
  bool IsPrerender() const;

  void RecordUKM();

  // content::WebContentsObserver:
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  WebContentsType cached_web_contents_type_ = WebContentsType::kUnknown;
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // Describes the current load.
  bool is_loading_ = false;
  NavigationType navigation_type_ = NavigationType::kNoCommit;

  // Counts loads since the last top-level navigation.
  std::array<int, static_cast<size_t>(NavigationType::kCount)> visible_loads_;
  std::array<int, static_cast<size_t>(NavigationType::kCount)> hidden_loads_;
};

PageLoadMetricsWebContentsObserver::PageLoadMetricsWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PageLoadMetricsWebContentsObserver>(
          *web_contents) {
  visible_loads_.fill(0);
  hidden_loads_.fill(0);
}

PageLoadMetricsWebContentsObserver::~PageLoadMetricsWebContentsObserver() {
  RecordUKM();
}

WebContentsType PageLoadMetricsWebContentsObserver::GetWebContentsType() {
  // The WebContents type cannot change from kTab, kExtension or kDevTools.
  if (cached_web_contents_type_ == WebContentsType::kTab ||
      cached_web_contents_type_ == WebContentsType::kExtension ||
      cached_web_contents_type_ == WebContentsType::kDevTools) {
    return cached_web_contents_type_;
  }

  if (IsTab()) {
    cached_web_contents_type_ = WebContentsType::kTab;
  } else if (IsExtension()) {
    cached_web_contents_type_ = WebContentsType::kExtension;
  } else if (IsDevTools()) {
    cached_web_contents_type_ = WebContentsType::kDevTools;
  } else if (IsPrerender()) {
    cached_web_contents_type_ = WebContentsType::kPrerender;
  }

  return cached_web_contents_type_;
}

bool PageLoadMetricsWebContentsObserver::IsTab() const {
#if BUILDFLAG(IS_ANDROID)
  return !!TabAndroid::FromWebContents(web_contents());
#else
  return !!chrome::FindBrowserWithTab(web_contents());
#endif
}

bool PageLoadMetricsWebContentsObserver::IsExtension() const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The process manager might be null for some irregular profiles, e.g. the
  // System Profile.
  if (extensions::ProcessManager* service = extensions::ProcessManager::Get(
          web_contents()->GetBrowserContext())) {
    return !!service->GetExtensionForWebContents(web_contents());
  }
#endif
  return false;
}

bool PageLoadMetricsWebContentsObserver::IsPrerender() const {
  auto* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (!no_state_prefetch_manager)
    return false;
  return no_state_prefetch_manager->IsWebContentsPrefetching(web_contents());
}

bool PageLoadMetricsWebContentsObserver::IsDevTools() const {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return DevToolsWindow::IsDevToolsWindow(web_contents());
#endif
}

void PageLoadMetricsWebContentsObserver::RecordUKM() {
  if (ukm_source_id_ != ukm::kInvalidSourceId) {
    ukm::builders::LoadCountsPerTopLevelDocument(ukm_source_id_)
        .SetNumMainFrameSameDocumentLoads_Visible(
            BucketizeLoadCount(visible_loads_[static_cast<size_t>(
                NavigationType::kMainFrameSameDocument)]))
        .SetNumMainFrameSameDocumentLoads_Hidden(
            BucketizeLoadCount(hidden_loads_[static_cast<size_t>(
                NavigationType::kMainFrameSameDocument)]))
        .SetNumSubFrameDifferentDocumentLoads_Visible(
            BucketizeLoadCount(visible_loads_[static_cast<size_t>(
                NavigationType::kSubFrameDifferentDocument)]))
        .SetNumSubFrameDifferentDocumentLoads_Hidden(
            BucketizeLoadCount(hidden_loads_[static_cast<size_t>(
                NavigationType::kSubFrameDifferentDocument)]))
        .SetNumSubFrameSameDocumentLoads_Visible(
            BucketizeLoadCount(visible_loads_[static_cast<size_t>(
                NavigationType::kSubFrameSameDocument)]))
        .SetNumSubFrameSameDocumentLoads_Hidden(
            BucketizeLoadCount(hidden_loads_[static_cast<size_t>(
                NavigationType::kSubFrameSameDocument)]))
        .Record(ukm::UkmRecorder::Get());
  }

  ukm_source_id_ = ukm::kInvalidSourceId;
  visible_loads_.fill(0);
  hidden_loads_.fill(0);
}

void PageLoadMetricsWebContentsObserver::DidStartLoading() {
  DCHECK(web_contents()->IsLoading());

  // TODO(crbug.com/40155922): Uncomment this DCHECK once there is a guarantee
  // that DidStartLoading and DidStopLoading are invoked in alternance.
  // DCHECK(!is_loading_);

  is_loading_ = true;
}

void PageLoadMetricsWebContentsObserver::DidStopLoading() {
  if (!is_loading_)
    return;

  const WebContentsType web_contents_type = GetWebContentsType();
  LoadType load_type;

  switch (web_contents_type) {
    case WebContentsType::kTab: {
      if (web_contents()->GetVisibility() == content::Visibility::VISIBLE) {
        load_type =
            static_cast<LoadType>(static_cast<int>(LoadType::kVisibleTabBase) +
                                  static_cast<int>(navigation_type_));
      } else {
        load_type =
            static_cast<LoadType>(static_cast<int>(LoadType::kHiddenTabBase) +
                                  static_cast<int>(navigation_type_));
      }
      break;
    }
    case WebContentsType::kPrerender: {
      load_type =
          static_cast<LoadType>(static_cast<int>(LoadType::kPrerenderBase) +
                                static_cast<int>(navigation_type_));
      break;
    }
    case WebContentsType::kExtension: {
      load_type = LoadType::kExtension;
      break;
    }
    case WebContentsType::kDevTools: {
      load_type = LoadType::kDevTools;
      break;
    }
    case WebContentsType::kUnknown: {
      load_type = LoadType::kUnknown;
      break;
    }
  }

  if (web_contents()->GetVisibility() == content::Visibility::VISIBLE)
    ++visible_loads_[static_cast<int>(navigation_type_)];
  else
    ++hidden_loads_[static_cast<int>(navigation_type_)];

  is_loading_ = false;
  navigation_type_ = NavigationType::kNoCommit;

  base::UmaHistogramEnumeration("Stability.Experimental.PageLoads", load_type);
}

void PageLoadMetricsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // We don't record metrics for prerendering pages.
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->GetRenderFrameHost()->IsActive()) {
    return;
  }

  DCHECK(is_loading_);

  if (navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    RecordUKM();
    ukm_source_id_ = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  }

  NavigationType navigation_type;
  if (navigation_handle->IsSameDocument()) {
    if (navigation_handle->IsInPrimaryMainFrame())
      navigation_type = NavigationType::kMainFrameSameDocument;
    else
      navigation_type = NavigationType::kSubFrameSameDocument;
  } else {
    if (navigation_handle->IsInPrimaryMainFrame())
      navigation_type = NavigationType::kMainFrameDifferentDocument;
    else
      navigation_type = NavigationType::kSubFrameDifferentDocument;
  }

  // Replace the navigation type of the current load only if the current
  // navigation has a lower value than previously seen navigations within the
  // current load.
  if (navigation_type < navigation_type_)
    navigation_type_ = navigation_type;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageLoadMetricsWebContentsObserver);

}  // namespace

PageLoadMetricsObserver::PageLoadMetricsObserver() {
  PerformanceManager::AddObserver(this);
}

PageLoadMetricsObserver::~PageLoadMetricsObserver() {
  PerformanceManager::RemoveObserver(this);
}

void PageLoadMetricsObserver::OnPageNodeCreatedForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  PageLoadMetricsWebContentsObserver::CreateForWebContents(web_contents);
}

}  // namespace performance_manager
