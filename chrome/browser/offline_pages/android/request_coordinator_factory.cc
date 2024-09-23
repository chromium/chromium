// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/request_coordinator_factory.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/offline_pages/android/background_scheduler_bridge.h"
#include "chrome/browser/offline_pages/android/load_termination_listener_impl.h"
#include "chrome/browser/offline_pages/background_loader_offliner.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/chrome_constants.h"
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

namespace {

class ActiveTabInfo : public RequestCoordinator::ActiveTabInfo {
 public:
  explicit ActiveTabInfo(Profile* profile) : profile_(profile) {}
  ~ActiveTabInfo() override {}
  bool DoesActiveTabMatch(const GURL& url) override {
    // Loop through to find the active tab and report whether the URL matches.
    for (const TabModel* model : TabModelList::models()) {
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
  raw_ptr<Profile> profile_;
};

}  // namespace

RequestCoordinatorFactory::RequestCoordinatorFactory()
    : ProfileKeyedServiceFactory(
          "OfflineRequestCoordinator",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  // Depends on OfflinePageModelFactory in SimpleDependencyManager.
}

// static
RequestCoordinatorFactory* RequestCoordinatorFactory::GetInstance() {
  static base::NoDestructor<RequestCoordinatorFactory> instance;
  return instance.get();
}

// static
RequestCoordinator* RequestCoordinatorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<RequestCoordinator*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

std::unique_ptr<KeyedService>
RequestCoordinatorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  std::unique_ptr<OfflinerPolicy> policy(new OfflinerPolicy());
  std::unique_ptr<Offliner> offliner;
  OfflinePageModel* model =
      OfflinePageModelFactory::GetInstance()->GetForBrowserContext(context);

  std::unique_ptr<LoadTerminationListenerImpl> load_termination_listener =
      std::make_unique<LoadTerminationListenerImpl>();
  offliner = std::make_unique<BackgroundLoaderOffliner>(
      context, policy.get(), model, std::move(load_termination_listener));

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
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
  return std::make_unique<RequestCoordinator>(
      std::move(policy), std::move(offliner), std::move(queue),
      std::move(scheduler), network_quality_tracker,
      std::make_unique<ActiveTabInfo>(profile));
}

}  // namespace offline_pages
