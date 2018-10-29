// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/page_signal_receiver.h"

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace resource_coordinator {

// static
bool PageSignalReceiver::IsEnabled() {
  // Check that service_manager is active and Resource Coordinator is enabled.
  return content::ServiceManagerConnection::GetForProcess() != nullptr;
}

// static
PageSignalReceiver* PageSignalReceiver::GetInstance() {
  if (!IsEnabled())
    return nullptr;

  static base::NoDestructor<PageSignalReceiver> page_signal_receiver;

  return page_signal_receiver.get();
}

PageSignalReceiver::PageSignalReceiver() : binding_(this) {}

PageSignalReceiver::~PageSignalReceiver() = default;

void PageSignalReceiver::NotifyPageAlmostIdle(
    const PageNavigationIdentity& page_navigation_id) {
  DCHECK(IsPageAlmostIdleSignalEnabled());
  NotifyObserversIfKnownCu(page_navigation_id,
                           &PageSignalObserver::OnPageAlmostIdle);
}

void PageSignalReceiver::NotifyRendererIsBloated(
    const PageNavigationIdentity& page_navigation_id) {
  NotifyObserversIfKnownCu(page_navigation_id,
                           &PageSignalObserver::OnRendererIsBloated);
}

void PageSignalReceiver::SetExpectedTaskQueueingDuration(
    const PageNavigationIdentity& page_navigation_id,
    base::TimeDelta duration) {
  NotifyObserversIfKnownCu(
      page_navigation_id,
      &PageSignalObserver::OnExpectedTaskQueueingDurationSet, duration);
}

void PageSignalReceiver::SetLifecycleState(
    const PageNavigationIdentity& page_navigation_id,
    mojom::LifecycleState state) {
  NotifyObserversIfKnownCu(page_navigation_id,
                           &PageSignalObserver::OnLifecycleStateChanged, state);
}

void PageSignalReceiver::NotifyNonPersistentNotificationCreated(
    const PageNavigationIdentity& page_navigation_id) {
  NotifyObserversIfKnownCu(
      page_navigation_id,
      &PageSignalObserver::OnNonPersistentNotificationCreated);
}

void PageSignalReceiver::OnLoadTimePerformanceEstimate(
    const PageNavigationIdentity& page_navigation_id,
    base::TimeDelta load_duration,
    base::TimeDelta cpu_usage_estimate,
    uint64_t private_footprint_kb_estimate) {
  NotifyObserversIfKnownCu(
      page_navigation_id, &PageSignalObserver::OnLoadTimePerformanceEstimate,
      load_duration, cpu_usage_estimate, private_footprint_kb_estimate);
}

void PageSignalReceiver::AddObserver(PageSignalObserver* observer) {
  // When PageSignalReceiver starts to have observer, construct the mojo
  // channel.
  if (!binding_.is_bound()) {
    content::ServiceManagerConnection* service_manager_connection =
        content::ServiceManagerConnection::GetForProcess();
    // Ensure service_manager is active before trying to connect to it.
    if (service_manager_connection) {
      service_manager::Connector* connector =
          service_manager_connection->GetConnector();
      mojom::PageSignalGeneratorPtr page_signal_generator_ptr;
      connector->BindInterface(mojom::kServiceName,
                               mojo::MakeRequest(&page_signal_generator_ptr));
      mojom::PageSignalReceiverPtr page_signal_receiver_ptr;
      binding_.Bind(mojo::MakeRequest(&page_signal_receiver_ptr));
      page_signal_generator_ptr->AddReceiver(
          std::move(page_signal_receiver_ptr));
    }
  }
  observers_.AddObserver(observer);
}

void PageSignalReceiver::RemoveObserver(PageSignalObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PageSignalReceiver::AssociateCoordinationUnitIDWithWebContents(
    const CoordinationUnitID& cu_id,
    content::WebContents* web_contents) {
  cu_id_web_contents_map_[cu_id] = web_contents;
}

void PageSignalReceiver::SetNavigationID(content::WebContents* web_contents,
                                         int64_t navigation_id) {
  DCHECK_NE(nullptr, web_contents);
  web_contents_navigation_id_map_[web_contents] = navigation_id;
}

void PageSignalReceiver::RemoveCoordinationUnitID(
    const CoordinationUnitID& cu_id) {
  auto it = cu_id_web_contents_map_.find(cu_id);
  DCHECK(it != cu_id_web_contents_map_.end());

  web_contents_navigation_id_map_.erase(it->second);
  cu_id_web_contents_map_.erase(it);
}

int64_t PageSignalReceiver::GetNavigationIDForWebContents(
    content::WebContents* web_contents) {
  DCHECK_NE(nullptr, web_contents);
  auto it = web_contents_navigation_id_map_.find(web_contents);
  if (it == web_contents_navigation_id_map_.end())
    return 0;

  return it->second;
}

template <typename Method, typename... Params>
void PageSignalReceiver::NotifyObserversIfKnownCu(
    const PageNavigationIdentity& page_navigation_id,
    Method m,
    Params... params) {
  auto web_contents_iter =
      cu_id_web_contents_map_.find(page_navigation_id.page_cu_id);
  if (web_contents_iter == cu_id_web_contents_map_.end())
    return;
  // An observer can make web_contents_iter invalid by mutating
  // the cu_id_web_contents_map_.
  content::WebContents* web_contents = web_contents_iter->second;
  for (auto& observer : observers_) {
    (observer.*m)(web_contents, page_navigation_id,
                  std::forward<Params>(params)...);
  }
}

}  // namespace resource_coordinator
