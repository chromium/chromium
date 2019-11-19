// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/request_coordinator_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/offline_pages/android/background_scheduler_bridge.h"
#include "chrome/browser/offline_pages/android/cct_request_observer.h"
#include "chrome/browser/offline_pages/android/load_termination_listener_impl.h"
#include "chrome/browser/offline_pages/background_loader_offliner.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_queue.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/scheduler.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "content/public/browser/web_contents.h"

namespace network {
class NetworkQualityTracker;
}

namespace offline_pages {

class ActiveTabInfo : public RequestCoordinator::ActiveTabInfo {
 public:
  explicit ActiveTabInfo(Profile* profile) : profile_(profile) {}
  ~ActiveTabInfo() override {}
  bool DoesActiveTabMatch(const GURL& url) override {
    // Loop through to find the active tab and report whether the URL matches.
    for (auto iter = TabModelList::begin(); iter != TabModelList::end();
         ++iter) {
      TabModel* model = *iter;
      if (model->GetProfile() == profile_) {
        content::WebContents* contents = model->GetActiveWebContents();
        // Check visibility to make sure Chrome is in the foreground.
        if (contents &&
            contents->GetVisibility() == content::Visibility::VISIBLE) {
          if (contents->GetVisibleURL() == url)
            return true;
          if (contents->GetLastCommittedURL() == url)
            return true;
        }
      }
    }
    return false;
  }

 private:
  Profile* profile_;
};

RequestCoordinatorFactory::RequestCoordinatorFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflineRequestCoordinator",
          BrowserContextDependencyManager::GetInstance()) {
  // Depends on OfflinePageModelFactory in SimpleDependencyManager.
}

// static
RequestCoordinatorFactory* RequestCoordinatorFactory::GetInstance() {
  return base::Singleton<RequestCoordinatorFactory>::get();
}

// static
RequestCoordinator* RequestCoordinatorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<RequestCoordinator*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* RequestCoordinatorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy());
  std::unique_ptr<Offliner> offliner;
  OfflinePageModel* model =
      OfflinePageModelFactory::GetInstance()->GetForBrowserContext(context);

  std::unique_ptr<LoadTerminationListenerImpl> load_termination_listener =
      std::make_unique<LoadTerminationListenerImpl>();
  offliner.reset(new BackgroundLoaderOffliner(
      context, policy.get(), model, std::move(load_termination_listener)));

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                       base::TaskPriority::BEST_EFFORT});
  Profile* profile = Profile::FromBrowserContext(context);
  base::FilePath queue_store_path =
      profile->GetPath().Append(chrome::kOfflinePageRequestQueueDirname);

  std::unique_ptr<RequestQueueStore> queue_store(
      new RequestQueueStore(background_task_runner, queue_store_path));
  std::unique_ptr<RequestQueue> queue(new RequestQueue(std::move(queue_store)));
  std::unique_ptr<Scheduler>
      scheduler(new android::BackgroundSchedulerBridge());
  network::NetworkQualityTracker* network_quality_tracker =
      g_browser_process->network_quality_tracker();
  RequestCoordinator* request_coordinator = new RequestCoordinator(
      std::move(policy), std::move(offliner), std::move(queue),
      std::move(scheduler), network_quality_tracker,
      std::make_unique<ActiveTabInfo>(profile));

  CCTRequestObserver::AttachToRequestCoordinator(request_coordinator);

  return request_coordinator;
}

}  // namespace offline_pages
