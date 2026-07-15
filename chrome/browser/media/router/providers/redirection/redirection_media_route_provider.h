// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_REDIRECTION_REDIRECTION_MEDIA_ROUTE_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_REDIRECTION_REDIRECTION_MEDIA_ROUTE_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/providers/redirection/redirection_service_host.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media_router {

// MediaRouteProvider for OS video redirection (MMR) through a single synthetic
// sink.
class RedirectionMediaRouteProvider : public mojom::MediaRouteProvider {
 public:
  static const mojom::MediaRouteProviderId kProviderId;

  RedirectionMediaRouteProvider(
      mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
      mojo::PendingRemote<mojom::MediaRouter> media_router);
  RedirectionMediaRouteProvider(const RedirectionMediaRouteProvider&) = delete;
  RedirectionMediaRouteProvider& operator=(
      const RedirectionMediaRouteProvider&) = delete;
  ~RedirectionMediaRouteProvider() override;

  // mojom::MediaRouteProvider:
  void CreateRoute(const std::string& media_source,
                   const std::string& sink_id,
                   const std::string& presentation_id,
                   const url::Origin& origin,
                   int32_t frame_tree_node_id,
                   base::TimeDelta timeout,
                   CreateRouteCallback callback) override;
  void JoinRoute(const std::string& media_source,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 int32_t frame_tree_node_id,
                 base::TimeDelta timeout,
                 JoinRouteCallback callback) override;
  void TerminateRoute(const std::string& route_id,
                      TerminateRouteCallback callback) override;
  void SendRouteMessage(const std::string& media_route_id,
                        const std::string& message) override;
  void SendRouteBinaryMessage(const std::string& media_route_id,
                              const std::vector<uint8_t>& data) override;
  void StartObservingMediaSinks(const std::string& media_source) override;
  void StopObservingMediaSinks(const std::string& media_source) override;
  void StartObservingMediaRoutes() override;
  void DetachRoute(const std::string& route_id) override;
  void DiscoverSinksNow() override;
  void BindMediaController(
      const std::string& route_id,
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer,
      BindMediaControllerCallback callback) override;
  void GetState(GetStateCallback callback) override;

 protected:
  // Returns true when OS redirection is available (i.e. inside an RDP session).
  // Virtual so tests can bypass the OS session check.
  virtual bool IsRedirectionAvailable() const;

  // Creates the host that owns a redirection session. Virtual so tests can
  // inject a host that does not launch a utility process.
  virtual std::unique_ptr<RedirectionServiceHost> CreateServiceHost();

 private:
  void TerminateActiveRoute();

  // Binds |this| to the Mojo receiver passed into the ctor.
  mojo::Receiver<mojom::MediaRouteProvider> receiver_;

  // Mojo remote to the Media Router.
  mojo::Remote<mojom::MediaRouter> media_router_;

  std::unique_ptr<RedirectionServiceHost> redirection_service_host_;

  // The single synthetic sink this provider exposes.
  MediaSinkInternal sink_;
  // Route ID of the currently active route, if any.
  std::string active_route_id_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RedirectionMediaRouteProvider> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_REDIRECTION_REDIRECTION_MEDIA_ROUTE_PROVIDER_H_
