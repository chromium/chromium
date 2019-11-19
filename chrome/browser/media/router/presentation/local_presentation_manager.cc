// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/local_presentation_manager.h"

#include <utility>

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using blink::mojom::PresentationInfo;

namespace media_router {

// LocalPresentationManager implementation.
LocalPresentationManager::LocalPresentationManager() {}

LocalPresentationManager::~LocalPresentationManager() {}

LocalPresentationManager::LocalPresentation*
LocalPresentationManager::GetOrCreateLocalPresentation(
    const PresentationInfo& presentation_info) {
  auto it = local_presentations_.find(presentation_info.id);
  if (it == local_presentations_.end()) {
    it = local_presentations_
             .insert(std::make_pair(
                 presentation_info.id,
                 std::make_unique<LocalPresentation>(presentation_info)))
             .first;
  }
  return it->second.get();
}

void LocalPresentationManager::RegisterLocalPresentationController(
    const PresentationInfo& presentation_info,
    const content::GlobalFrameRoutingId& render_frame_host_id,
    mojo::PendingRemote<blink::mojom::PresentationConnection>
        controller_connection_remote,
    mojo::PendingReceiver<blink::mojom::PresentationConnection>
        receiver_connection_receiver,
    const MediaRoute& route) {
  DVLOG(2) << __func__ << " [presentation_id]: " << presentation_info.id
           << ", [render_frame_host_id]: "
           << render_frame_host_id.frame_routing_id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto* presentation = GetOrCreateLocalPresentation(presentation_info);
  presentation->RegisterController(
      render_frame_host_id, std::move(controller_connection_remote),
      std::move(receiver_connection_receiver), route);
}

void LocalPresentationManager::UnregisterLocalPresentationController(
    const std::string& presentation_id,
    const content::GlobalFrameRoutingId& render_frame_host_id) {
  DVLOG(2) << __func__ << " [presentation_id]: " << presentation_id
           << ", [render_frame_host_id]: "
           << render_frame_host_id.frame_routing_id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = local_presentations_.find(presentation_id);
  if (it == local_presentations_.end())
    return;

  // Remove presentation if no controller and receiver.
  it->second->UnregisterController(render_frame_host_id);
  if (!it->second->IsValid()) {
    DLOG(WARNING) << __func__ << " no receiver callback has been registered to "
                  << "[presentation_id]: " << presentation_id;
    local_presentations_.erase(presentation_id);
  }
}

void LocalPresentationManager::OnLocalPresentationReceiverCreated(
    const PresentationInfo& presentation_info,
    const content::ReceiverConnectionAvailableCallback& receiver_callback) {
  DVLOG(2) << __func__ << " [presentation_id]: " << presentation_info.id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* presentation = GetOrCreateLocalPresentation(presentation_info);
  presentation->RegisterReceiver(receiver_callback);
}

void LocalPresentationManager::OnLocalPresentationReceiverTerminated(
    const std::string& presentation_id) {
  DVLOG(2) << __func__ << " [presentation_id]: " << presentation_id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  local_presentations_.erase(presentation_id);
}

bool LocalPresentationManager::IsLocalPresentation(
    const std::string& presentation_id) {
  return base::Contains(local_presentations_, presentation_id);
}

const MediaRoute* LocalPresentationManager::GetRoute(
    const std::string& presentation_id) {
  auto it = local_presentations_.find(presentation_id);
  return (it != local_presentations_.end() && it->second->route_.has_value())
             ? &(it->second->route_.value())
             : nullptr;
}

// LocalPresentation implementation.
LocalPresentationManager::LocalPresentation::LocalPresentation(
    const PresentationInfo& presentation_info)
    : presentation_info_(presentation_info) {}

LocalPresentationManager::LocalPresentation::~LocalPresentation() {}

void LocalPresentationManager::LocalPresentation::RegisterController(
    const content::GlobalFrameRoutingId& render_frame_host_id,
    mojo::PendingRemote<blink::mojom::PresentationConnection>
        controller_connection_remote,
    mojo::PendingReceiver<blink::mojom::PresentationConnection>
        receiver_connection_receiver,
    const MediaRoute& route) {
  if (!receiver_callback_.is_null()) {
    receiver_callback_.Run(PresentationInfo::New(presentation_info_),
                           std::move(controller_connection_remote),
                           std::move(receiver_connection_receiver));
  } else {
    pending_controllers_.insert(std::make_pair(
        render_frame_host_id, std::make_unique<ControllerConnection>(
                                  std::move(controller_connection_remote),
                                  std::move(receiver_connection_receiver))));
  }

  route_ = route;
}

void LocalPresentationManager::LocalPresentation::UnregisterController(
    const content::GlobalFrameRoutingId& render_frame_host_id) {
  pending_controllers_.erase(render_frame_host_id);
}

void LocalPresentationManager::LocalPresentation::RegisterReceiver(
    const content::ReceiverConnectionAvailableCallback& receiver_callback) {
  DCHECK(receiver_callback_.is_null());

  for (auto& controller : pending_controllers_) {
    receiver_callback.Run(
        PresentationInfo::New(presentation_info_),
        std::move(controller.second->controller_connection_remote),
        std::move(controller.second->receiver_connection_receiver));
  }
  receiver_callback_ = receiver_callback;
  pending_controllers_.clear();
}

bool LocalPresentationManager::LocalPresentation::IsValid() const {
  return !(pending_controllers_.empty() && receiver_callback_.is_null());
}

LocalPresentationManager::LocalPresentation::ControllerConnection::
    ControllerConnection(
        mojo::PendingRemote<blink::mojom::PresentationConnection>
            controller_connection_remote,
        mojo::PendingReceiver<blink::mojom::PresentationConnection>
            receiver_connection_receiver)
    : controller_connection_remote(std::move(controller_connection_remote)),
      receiver_connection_receiver(std::move(receiver_connection_receiver)) {}

LocalPresentationManager::LocalPresentation::ControllerConnection::
    ~ControllerConnection() {}

}  // namespace media_router
