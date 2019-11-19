// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;

performance_manager::TabVisibility ContentVisibilityToRCVisibility(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE)
    return performance_manager::TabVisibility::kForeground;
  return performance_manager::TabVisibility::kBackground;
}

}  // namespace

class LocalSiteCharacteristicsWebContentsObserver::GraphObserver
    : public performance_manager::FrameNode::ObserverDefaultImpl,
      public performance_manager::GraphOwned {
 public:
  using FrameNode = performance_manager::FrameNode;
  using Graph = performance_manager::Graph;
  using WebContentsProxy = performance_manager::WebContentsProxy;

  GraphObserver();

  // FrameNodeObserver implementation:
  void OnNonPersistentNotificationCreated(const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override {
    graph->AddFrameNodeObserver(this);
  }
  void OnTakenFromGraph(Graph* graph) override {
    graph->RemoveFrameNodeObserver(this);
  }

 private:
  static void DispatchNonPersistentNotificationCreated(
      WebContentsProxy contents_proxy,
      const url::Origin& origin);

  // Binds to the task runner where the object is constructed.
  scoped_refptr<base::SequencedTaskRunner> destination_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

LocalSiteCharacteristicsWebContentsObserver::
    LocalSiteCharacteristicsWebContentsObserver(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // May not be present in some tests.
  if (performance_manager::PerformanceManagerImpl::IsAvailable()) {
    // The performance manager has to be enabled in order to properly track the
    // non-persistent notification events.
    TabLoadTracker::Get()->AddObserver(this);
  }
}

LocalSiteCharacteristicsWebContentsObserver::
    ~LocalSiteCharacteristicsWebContentsObserver() {
  DCHECK(!writer_);
}

// static
void LocalSiteCharacteristicsWebContentsObserver::MaybeCreateGraphObserver() {
  if (performance_manager::PerformanceManagerImpl::IsAvailable()) {
    performance_manager::PerformanceManagerImpl::PassToGraph(
        FROM_HERE, std::make_unique<GraphObserver>());
  }
}

void LocalSiteCharacteristicsWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!writer_)
    return;

  auto rc_visibility = ContentVisibilityToRCVisibility(visibility);
  UpdateBackgroundedTime(rc_visibility);
  writer_->NotifySiteVisibilityChanged(rc_visibility);
}

void LocalSiteCharacteristicsWebContentsObserver::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (performance_manager::PerformanceManagerImpl::IsAvailable())
    TabLoadTracker::Get()->RemoveObserver(this);
  writer_.reset();
  writer_origin_ = url::Origin();
}

void LocalSiteCharacteristicsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);

  // Ignore the navigation events happening in a subframe of in the same
  // document.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  first_time_title_set_ = false;
  first_time_favicon_set_ = false;

  if (!navigation_handle->HasCommitted())
    return;

  const url::Origin new_origin =
      url::Origin::Create(navigation_handle->GetURL());

  if (writer_ && new_origin == writer_origin_)
    return;

  writer_.reset();
  writer_origin_ = url::Origin();

  if (!URLShouldBeStoredInLocalDatabase(navigation_handle->GetURL()))
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  DCHECK(profile);
  SiteCharacteristicsDataStore* data_store =
      LocalSiteCharacteristicsDataStoreFactory::GetForProfile(profile);

  // A data store might not be available in some unit tests.
  if (data_store) {
    auto rc_visibility =
        ContentVisibilityToRCVisibility(web_contents()->GetVisibility());
    writer_ = data_store->GetWriterForOrigin(new_origin, rc_visibility);
    UpdateBackgroundedTime(rc_visibility);
  }

  // The writer is initially in an unloaded state, load it if necessary.
  if (TabLoadTracker::Get()->GetLoadingState(web_contents()) ==
      LoadingState::LOADED) {
    OnSiteLoaded();
  }

  writer_origin_ = new_origin;
}

void LocalSiteCharacteristicsWebContentsObserver::TitleWasSet(
    content::NavigationEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sebmarchand): Check if the title is always set at least once before
  // loading completes, in which case this check could be removed.
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUpdatesTitleInBackground,
      FeatureType::kTitleChange);
}

void LocalSiteCharacteristicsWebContentsObserver::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUpdatesFaviconInBackground,
      FeatureType::kFaviconChange);
}

void LocalSiteCharacteristicsWebContentsObserver::OnAudioStateChanged(
    bool audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!audible)
    return;

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUsesAudioInBackground,
      FeatureType::kAudioUsage);
}

void LocalSiteCharacteristicsWebContentsObserver::OnLoadingStateChange(
    content::WebContents* contents,
    LoadingState old_loading_state,
    LoadingState new_loading_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_contents() != contents)
    return;

  if (!writer_)
    return;

  // Ignore the transitions from/to an UNLOADED state.
  if (new_loading_state == LoadingState::LOADED) {
    OnSiteLoaded();
  } else if (old_loading_state == LoadingState::LOADED) {
    writer_->NotifySiteUnloaded();
    loaded_time_ = base::TimeTicks();
  }
}

void LocalSiteCharacteristicsWebContentsObserver::
    OnNonPersistentNotificationCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(performance_manager::PerformanceManagerImpl::IsAvailable());

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUsesNotificationsInBackground,
      FeatureType::kNotificationUsage);
}

bool LocalSiteCharacteristicsWebContentsObserver::ShouldIgnoreFeatureUsageEvent(
    FeatureType feature_type) {
  // The feature usage should be ignored if there's no writer for this tab.
  if (!writer_)
    return true;

  // Ignore all features happening before the website gets fully loaded except
  // for the non-persistent notifications.
  if (feature_type != FeatureType::kNotificationUsage &&
      TabLoadTracker::Get()->GetLoadingState(web_contents()) !=
          LoadingState::LOADED) {
    return true;
  }

  // Ignore events if the tab is not in background.
  if (ContentVisibilityToRCVisibility(web_contents()->GetVisibility()) !=
      performance_manager::TabVisibility::kBackground) {
    return true;
  }

  if (feature_type == FeatureType::kTitleChange ||
      feature_type == FeatureType::kFaviconChange) {
    DCHECK(!loaded_time_.is_null());
    if (NowTicks() - loaded_time_ <
        GetStaticSiteCharacteristicsDatabaseParams()
            .title_or_favicon_change_post_load_grace_period) {
      return true;
    }
  }

  // Ignore events happening shortly after the tab being backgrounded, they're
  // usually false positives.
  DCHECK(!backgrounded_time_.is_null());
  if (NowTicks() - backgrounded_time_ <
      GetStaticSiteCharacteristicsDatabaseParams()
          .feature_usage_post_background_grace_period) {
    return true;
  }

  return false;
}

void LocalSiteCharacteristicsWebContentsObserver::
    MaybeNotifyBackgroundFeatureUsage(
        void (SiteCharacteristicsDataWriter::*method)(),
        FeatureType feature_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldIgnoreFeatureUsageEvent(feature_type))
    return;

  (writer_.get()->*method)();
}

void LocalSiteCharacteristicsWebContentsObserver::OnSiteLoaded() {
  DCHECK(writer_);
  writer_->NotifySiteLoaded();
  loaded_time_ = NowTicks();
}

void LocalSiteCharacteristicsWebContentsObserver::UpdateBackgroundedTime(
    performance_manager::TabVisibility visibility) {
  if (visibility == performance_manager::TabVisibility::kBackground) {
    backgrounded_time_ = NowTicks();
  } else {
    backgrounded_time_ = base::TimeTicks();
  }
}

LocalSiteCharacteristicsWebContentsObserver::GraphObserver::GraphObserver()
    : destination_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  // This object will be used on the PM sequence hereafter.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void LocalSiteCharacteristicsWebContentsObserver::GraphObserver::
    OnNonPersistentNotificationCreated(const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const performance_manager::PageNode* page_node = frame_node->GetPageNode();
  destination_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GraphObserver::DispatchNonPersistentNotificationCreated,
          page_node->GetContentsProxy(),
          url::Origin::Create(page_node->GetMainFrameUrl().GetOrigin())));
}

namespace {

// Return the WCO if notification is not late, and it's available.
LocalSiteCharacteristicsWebContentsObserver* MaybeGetWCO(
    performance_manager::WebContentsProxy contents_proxy,
    const url::Origin& origin) {
  content::WebContents* web_contents = contents_proxy.Get();
  // The WC may be dead by the time the task posted from the PM sequence arrives
  // on the UI thread.
  if (!web_contents)
    return nullptr;

  // The L41r is not itself WebContentsUserData, but rather stored on
  // the RC TabHelper, so retrieve that first - if available.
  ResourceCoordinatorTabHelper* rc_th =
      ResourceCoordinatorTabHelper::FromWebContents(web_contents);
  if (!rc_th)
    return nullptr;

  auto* wco = rc_th->local_site_characteristics_wc_observer();
  if (wco->writer_origin() != origin)
    return nullptr;

  return wco;
}

}  // namespace

// static
void LocalSiteCharacteristicsWebContentsObserver::GraphObserver::
    DispatchNonPersistentNotificationCreated(WebContentsProxy contents_proxy,
                                             const url::Origin& origin) {
  if (auto* wco = MaybeGetWCO(contents_proxy, origin))
    wco->OnNonPersistentNotificationCreated();
}

}  // namespace resource_coordinator
