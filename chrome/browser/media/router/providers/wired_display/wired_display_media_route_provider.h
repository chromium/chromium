// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_MEDIA_ROUTE_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_MEDIA_ROUTE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_presentation_receiver.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/mojom/media_status.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"

class Profile;

namespace media_router {

class WiredDisplayPresentationReceiver;

// A MediaRouteProvider class that provides wired displays as media sinks.
// Displays can be used as sinks when there are multiple dipslays that are not
// mirrored.
class WiredDisplayMediaRouteProvider : public mojom::MediaRouteProvider,
                                       public display::DisplayObserver {
 public:
  static const mojom::MediaRouteProviderId kProviderId;

  static std::string GetSinkIdForDisplay(const display::Display& display);

  static std::string GetRouteDescription(const std::string& media_source);

  WiredDisplayMediaRouteProvider(
      mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
      mojo::PendingRemote<mojom::MediaRouter> media_router,
      Profile* profile);

  WiredDisplayMediaRouteProvider(const WiredDisplayMediaRouteProvider&) =
      delete;
  WiredDisplayMediaRouteProvider& operator=(
      const WiredDisplayMediaRouteProvider&) = delete;

  ~WiredDisplayMediaRouteProvider() override;

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
  void EnableMdnsDiscovery() override;
  void DiscoverSinksNow() override;
  void BindMediaController(
      const std::string& route_id,
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer,
      BindMediaControllerCallback callback) override;
  void GetState(GetStateCallback callback) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 protected:
  // Returns all displays, including those that may not be used as sinks.
  virtual std::vector<display::Display> GetAllDisplays() const;

  virtual display::Display GetPrimaryDisplay() const;

 private:
  class Presentation {
   public:
    explicit Presentation(const MediaRoute& route);
    Presentation(Presentation&& other);

    Presentation(const Presentation&) = delete;
    Presentation& operator=(const Presentation&) = delete;

    ~Presentation();

    // Updates the title for the presentation page, and notifies media status
    // observers if the title changed.
    void UpdatePresentationTitle(const std::string& title);

    void SetMojoConnections(
        mojo::PendingReceiver<mojom::MediaController> media_controller,
        mojo::PendingRemote<mojom::MediaStatusObserver> observer);

    // Resets the Mojo connections to media controller and status observer.
    void ResetMojoConnections();

    const MediaRoute& route() const { return route_; }

    WiredDisplayPresentationReceiver* receiver() const {
      return receiver_.get();
    }

    void set_receiver(
        std::unique_ptr<WiredDisplayPresentationReceiver> receiver) {
      receiver_ = std::move(receiver);
    }

   private:
    MediaRoute route_;
    std::unique_ptr<WiredDisplayPresentationReceiver> receiver_;
    mojom::MediaStatusPtr status_;

    // |media_controller_receiver_| is retained but not used.
    mojo::PendingReceiver<mojom::MediaController> media_controller_receiver_;

    // |media_status_observer|, when set, gets notified whenever |status|
    // changes.
    mojo::Remote<mojom::MediaStatusObserver> media_status_observer_;
  };

  using Presentations = std::map<std::string, Presentation>;

  // Sends the current list of routes to each query in |route_queries_|.
  void NotifyRouteObservers() const;

  // Sends the current list of sinks to each query in |sink_queries_|.
  void NotifySinkObservers();

  // Removes the presentation from |presentations_| and notifies route
  // observers.
  void RemovePresentationById(const std::string& presentation_id);

  std::unique_ptr<WiredDisplayPresentationReceiver> CreatePresentationReceiver(
      const std::string& presentation_id,
      Presentation* presentation,
      const display::Display& display);

  // Terminates all presentation receivers on |display|.
  void TerminatePresentationsOnDisplay(const display::Display& display);

  // Returns a display associated with |sink_id|, or a nullopt if not found.
  std::optional<display::Display> GetDisplayBySinkId(
      const std::string& sink_id) const;

  // Returns a list of available sinks.
  std::vector<MediaSinkInternal> GetSinks() const;

  // Returns a list of displays that can be used as sinks. Returns an empty list
  // if there is only one display or if the secondary displays mirror the
  // primary display.
  std::vector<display::Display> GetAvailableDisplays() const;

  // Binds |this| to the Mojo receiver passed into the ctor.
  mojo::Receiver<mojom::MediaRouteProvider> receiver_;

  // Mojo remote to the Media Router.
  mojo::Remote<mojom::MediaRouter> media_router_;

  // Presentation profiles are created based on this original profile. This
  // profile is not owned by |this|.
  raw_ptr<Profile> profile_;

  // Map from presentation IDs to active presentations managed by this provider.
  Presentations presentations_;

  // A set of MediaSource IDs associated with queries for MediaSink updates.
  base::flat_set<std::string> sink_queries_;

  // Used for recording UMA metrics for the number of sinks available.
  WiredDisplayDeviceCountMetrics device_count_metrics_;

  std::optional<display::ScopedDisplayObserver> display_observer_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_MEDIA_ROUTE_PROVIDER_H_
