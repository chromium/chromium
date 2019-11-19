// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_MEDIA_ROUTE_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_MEDIA_ROUTE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_presentation_receiver.h"
#include "chrome/common/media_router/media_route_provider_helper.h"
#include "chrome/common/media_router/mojom/media_router.mojom.h"
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
  static const MediaRouteProviderId kProviderId;

  static std::string GetSinkIdForDisplay(const display::Display& display);

  static std::string GetRouteDescription(const std::string& media_source);

  WiredDisplayMediaRouteProvider(
      mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
      mojo::PendingRemote<mojom::MediaRouter> media_router,
      Profile* profile);
  ~WiredDisplayMediaRouteProvider() override;

  // mojom::MediaRouteProvider:
  void CreateRoute(const std::string& media_source,
                   const std::string& sink_id,
                   const std::string& presentation_id,
                   const url::Origin& origin,
                   int32_t tab_id,
                   base::TimeDelta timeout,
                   bool incognito,
                   CreateRouteCallback callback) override;
  void JoinRoute(const std::string& media_source,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 int32_t tab_id,
                 base::TimeDelta timeout,
                 bool incognito,
                 JoinRouteCallback callback) override;
  void ConnectRouteByRouteId(const std::string& media_source,
                             const std::string& route_id,
                             const std::string& presentation_id,
                             const url::Origin& origin,
                             int32_t tab_id,
                             base::TimeDelta timeout,
                             bool incognito,
                             ConnectRouteByRouteIdCallback callback) override;
  void TerminateRoute(const std::string& route_id,
                      TerminateRouteCallback callback) override;
  void SendRouteMessage(const std::string& media_route_id,
                        const std::string& message) override;
  void SendRouteBinaryMessage(const std::string& media_route_id,
                              const std::vector<uint8_t>& data) override;
  void StartObservingMediaSinks(const std::string& media_source) override;
  void StopObservingMediaSinks(const std::string& media_source) override;
  void StartObservingMediaRoutes(const std::string& media_source) override;
  void StopObservingMediaRoutes(const std::string& media_source) override;
  void StartListeningForRouteMessages(const std::string& route_id) override;
  void StopListeningForRouteMessages(const std::string& route_id) override;
  void DetachRoute(const std::string& route_id) override;
  void EnableMdnsDiscovery() override;
  void UpdateMediaSinks(const std::string& media_source) override;
  void SearchSinks(const std::string& sink_id,
                   const std::string& media_source,
                   mojom::SinkSearchCriteriaPtr search_criteria,
                   SearchSinksCallback callback) override;
  void ProvideSinks(
      const std::string& provider_name,
      const std::vector<media_router::MediaSinkInternal>& sinks) override;
  void CreateMediaRouteController(
      const std::string& route_id,
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer,
      CreateMediaRouteControllerCallback callback) override;

  // display::DisplayObserver:
  void OnDidProcessDisplayChanges() override;
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
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

    DISALLOW_COPY_AND_ASSIGN(Presentation);
  };

  // Sends the current list of routes to each query in |route_queries_|.
  void NotifyRouteObservers() const;

  // Sends the current list of sinks to each query in |sink_queries_|.
  void NotifySinkObservers();

  // Notifies |media_router_| of the current sink availability.
  void ReportSinkAvailability(const std::vector<MediaSinkInternal>& sinks);

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
  base::Optional<display::Display> GetDisplayBySinkId(
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
  Profile* profile_;

  // Map from presentation IDs to active presentations managed by this provider.
  std::map<std::string, Presentation> presentations_;

  // A set of MediaSource IDs associated with queries for MediaRoute updates.
  base::flat_set<std::string> route_queries_;

  // A set of MediaSource IDs associated with queries for MediaSink updates.
  base::flat_set<std::string> sink_queries_;

  // Used for recording UMA metrics for the number of sinks available.
  WiredDisplayDeviceCountMetrics device_count_metrics_;

  // Keeps track of whether |this| is registered with display::Screen as a
  // DisplayObserver.
  bool is_observing_displays_ = false;

  DISALLOW_COPY_AND_ASSIGN(WiredDisplayMediaRouteProvider);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_WIRED_DISPLAY_WIRED_DISPLAY_MEDIA_ROUTE_PROVIDER_H_
