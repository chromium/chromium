// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/page_metadata_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "components/optimization_guide/content/browser/page_content_metadata_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"

namespace glic {

struct PageMetadataManager::PageMetadataSubscription {
  PageMetadataSubscription(
      std::unique_ptr<optimization_guide::PageContentMetadataObserver> observer,
      base::CallbackListSubscription will_detach_subscription,
      base::CallbackListSubscription will_discard_contents_subscription,
      const std::vector<std::string>& names);
  ~PageMetadataSubscription();
  PageMetadataSubscription(PageMetadataSubscription&&);
  PageMetadataSubscription& operator=(PageMetadataSubscription&&);

  std::unique_ptr<optimization_guide::PageContentMetadataObserver> observer;
  base::CallbackListSubscription will_detach_subscription;
  base::CallbackListSubscription will_discard_contents_subscription;
  std::vector<std::string> names;
};

PageMetadataManager::PageMetadataSubscription::PageMetadataSubscription(
    std::unique_ptr<optimization_guide::PageContentMetadataObserver> observer,
    base::CallbackListSubscription will_detach_subscription,
    base::CallbackListSubscription will_discard_contents_subscription,
    const std::vector<std::string>& names)
    : observer(std::move(observer)),
      will_detach_subscription(std::move(will_detach_subscription)),
      will_discard_contents_subscription(
          std::move(will_discard_contents_subscription)),
      names(names) {}

PageMetadataManager::PageMetadataSubscription::~PageMetadataSubscription() =
    default;
PageMetadataManager::PageMetadataSubscription::PageMetadataSubscription(
    PageMetadataSubscription&&) = default;
PageMetadataManager::PageMetadataSubscription&
PageMetadataManager::PageMetadataSubscription::operator=(
    PageMetadataSubscription&&) = default;

PageMetadataManager::PageMetadataManager(glic::mojom::WebClient* web_client)
    : web_client_(web_client) {}

PageMetadataManager::~PageMetadataManager() = default;

void PageMetadataManager::SubscribeToPageMetadata(
    int32_t tab_id,
    const std::vector<std::string>& names,
    glic::mojom::WebClientHandler::SubscribeToPageMetadataCallback callback) {
  // Erase any existing subscription for this tab.
  tab_id_to_page_metadata_subscriptions_.erase(tab_id);

  if (names.empty()) {
    // An empty name list is an unsubscription. We've already erased the
    // old subscription, so we're done.
    std::move(callback).Run(true);
    return;
  }

  tabs::TabHandle tab_handle(tab_id);
  auto* tab = tab_handle.Get();
  if (!tab) {
    std::move(callback).Run(false);
    return;
  }
  content::WebContents* web_contents = tab->GetContents();
  if (!web_contents || web_contents->IsBeingDestroyed()) {
    std::move(callback).Run(false);
    return;
  }

  auto on_page_metadata_changed =
      base::BindRepeating(&PageMetadataManager::NotifyPageMetadataChanged,
                          base::Unretained(this), tab_id);

  auto observer =
      std::make_unique<optimization_guide::PageContentMetadataObserver>(
          web_contents, names, std::move(on_page_metadata_changed));

  auto will_detach_subscription = tab->RegisterWillDetach(base::BindRepeating(
      &PageMetadataManager::OnTabWillDetach, base::Unretained(this)));

  auto will_discard_contents_subscription = tab->RegisterWillDiscardContents(
      base::BindRepeating(&PageMetadataManager::OnTabWillDiscardContents,
                          base::Unretained(this)));

  tab_id_to_page_metadata_subscriptions_.emplace(
      tab_id, PageMetadataSubscription{
                  std::move(observer), std::move(will_detach_subscription),
                  std::move(will_discard_contents_subscription), names});

  std::move(callback).Run(true);
}

void PageMetadataManager::OnTabWillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  const int32_t tab_id = tab->GetHandle().raw_value();
  auto it = tab_id_to_page_metadata_subscriptions_.find(tab_id);
  if (it == tab_id_to_page_metadata_subscriptions_.end()) {
    return;
  }

  auto& subscription = it->second;
  if (!new_contents || new_contents->IsBeingDestroyed()) {
    // The observer is tied to the old web contents and will be destroyed.
    // Since there's no new web contents, we can't create a new observer.
    // The subscription will be removed by OnTabWillDetach.
    subscription.observer.reset();
    return;
  }

  auto on_page_metadata_changed =
      base::BindRepeating(&PageMetadataManager::NotifyPageMetadataChanged,
                          base::Unretained(this), tab_id);

  auto observer =
      std::make_unique<optimization_guide::PageContentMetadataObserver>(
          new_contents, subscription.names,
          std::move(on_page_metadata_changed));
  subscription.observer = std::move(observer);
}

void PageMetadataManager::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason != tabs::TabInterface::DetachReason::kDelete) {
    return;
  }
  const int32_t tab_id = tab->GetHandle().raw_value();
  NotifyPageMetadataChanged(tab_id, nullptr);
  tab_id_to_page_metadata_subscriptions_.erase(tab_id);
}

void PageMetadataManager::NotifyPageMetadataChanged(
    int32_t tab_id,
    blink::mojom::PageMetadataPtr page_metadata) {
  web_client_->NotifyPageMetadataChanged(tab_id, std::move(page_metadata));
}

}  // namespace glic
