// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DESKTOP_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DESKTOP_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/mojo/media_router_debugger_impl.h"
#include "chrome/browser/media/router/mojo/media_sink_service_status.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "chrome/browser/media/router/providers/dial/dial_media_route_provider.h"
#include "components/media_router/browser/issue_manager.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/browser/media_router_base.h"
#include "components/media_router/browser/media_router_debugger.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/mirroring_media_controller_host.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/mojom/logger.mojom.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/route_request_result.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

class DesktopMediaPickerController;

namespace content {
class BrowserContext;
struct DesktopMediaID;
}

namespace media {
class FlingingController;
}

namespace media_router {

class CastMediaRouteProvider;
class DualMediaSinkService;
class WiredDisplayMediaRouteProvider;

// MediaRouter implementation that uses the desktop MediaRouteProviders.
class MediaRouterDesktop : public MediaRouterBase, public mojom::MediaRouter {
 public:
  explicit MediaRouterDesktop(content::BrowserContext* context);
  MediaRouterDesktop(const MediaRouterDesktop&) = delete;
  MediaRouterDesktop& operator=(const MediaRouterDesktop&) = delete;
  MediaRouterDesktop(const MediaRouterDesktop&&) = delete;
  MediaRouterDesktop& operator=(const MediaRouterDesktop&&) = delete;

  ~MediaRouterDesktop() override;

 private:
  // ::media_router::MediaRouter implementation:
  void Initialize() final;
  void CreateRoute(const MediaSource::Id& source_id,
                   const MediaSink::Id& sink_id,
                   const url::Origin& origin,
                   content::WebContents* web_contents,
                   MediaRouteResponseCallback callback,
                   base::TimeDelta timeout) final;
  void JoinRoute(const MediaSource::Id& source_id,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 content::WebContents* web_contents,
                 MediaRouteResponseCallback callback,
                 base::TimeDelta timeout) final;
  void TerminateRoute(const MediaRoute::Id& route_id) final;
  // TODO(crbug.com/40177419): Remove DetachRoute(), SendRouteMessage(),
  // and SendRouteBinaryMessage().
  void DetachRoute(MediaRoute::Id route_id) final;
  void SendRouteMessage(const MediaRoute::Id& route_id,
                        const std::string& message) final;
  void SendRouteBinaryMessage(const MediaRoute::Id& route_id,
                              std::unique_ptr<std::vector<uint8_t>> data) final;
  void OnUserGesture() final;
  std::vector<MediaRoute> GetCurrentRoutes() const final;
  std::unique_ptr<media::FlingingController> GetFlingingController(
      const MediaRoute::Id& route_id) final;
  MirroringMediaControllerHost* GetMirroringMediaControllerHost(
      const MediaRoute::Id& route_id) final;
  IssueManager* GetIssueManager() final;
  void GetMediaController(
      const MediaRoute::Id& route_id,
      mojo::PendingReceiver<mojom::MediaController> controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) final;
  base::Value GetLogs() const final;
  base::Value::Dict GetState() const override;
  void GetProviderState(
      mojom::MediaRouteProviderId provider_id,
      mojom::MediaRouteProvider::GetStateCallback callback) const override;
  LoggerImpl* GetLogger() final;
  MediaRouterDebugger& GetDebugger() final;
  bool RegisterMediaSinksObserver(MediaSinksObserver* observer) final;
  void UnregisterMediaSinksObserver(MediaSinksObserver* observer) final;
  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) final;
  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) final;
  void RegisterPresentationConnectionMessageObserver(
      PresentationConnectionMessageObserver* observer) final;
  void UnregisterPresentationConnectionMessageObserver(
      PresentationConnectionMessageObserver* observer) final;

  // ::media_router::mojom::MediaRouter implementation:
  // TODO(crbug.com/40177419): Remove RegisterMediaRouteProvider().
  void RegisterMediaRouteProvider(mojom::MediaRouteProviderId provider_id,
                                  mojo::PendingRemote<mojom::MediaRouteProvider>
                                      media_route_provider_remote) final;
  void OnSinksReceived(mojom::MediaRouteProviderId provider_id,
                       const std::string& media_source,
                       const std::vector<MediaSinkInternal>& internal_sinks,
                       const std::vector<url::Origin>& origins) final;
  void OnIssue(const IssueInfo& issue) final;
  void ClearTopIssueForSink(const MediaSink::Id& sink_id) final;
  void OnRoutesUpdated(mojom::MediaRouteProviderId provider_id,
                       const std::vector<MediaRoute>& routes) final;
  // TODO(crbug.com/40177419): Remove
  // OnPresentationConnectionStateChanged(), OnPresentationConnectionClosed(),
  // and OnRouteMessagesReceived().
  void OnPresentationConnectionStateChanged(
      const std::string& route_id,
      blink::mojom::PresentationConnectionState state) final;
  void OnPresentationConnectionClosed(
      const std::string& route_id,
      blink::mojom::PresentationConnectionCloseReason reason,
      const std::string& message) final;
  void OnRouteMessagesReceived(
      const std::string& route_id,
      std::vector<mojom::RouteMessagePtr> messages) final;
  void GetMediaSinkServiceStatus(
      mojom::MediaRouter::GetMediaSinkServiceStatusCallback callback) final;
  // TODO(mfoltz): For GetLogger and GetDebugger(), it is confusing to have a
  // mojo API overload a non-mojo API.   Rename one or the other.
  void GetLogger(mojo::PendingReceiver<mojom::Logger> receiver) final;
  void GetDebugger(mojo::PendingReceiver<mojom::Debugger> receiver) final;
  void GetLogsAsString(GetLogsAsStringCallback callback) final;

  // ::KeyedService implementation.
  void Shutdown() override;

  // Result callback when Mojo TerminateRoute is invoked.
  // `route_id`: ID of MediaRoute passed to the TerminateRoute request.
  // `provider_id`: ID of MediaRouteProvider that handled the request.
  // `error_text`: Error message if an error occurred.
  // `result_code`: The result of the request.
  void OnTerminateRouteResult(const MediaRoute::Id& route_id,
                              mojom::MediaRouteProviderId provider_id,
                              const std::optional<std::string>& error_text,
                              mojom::RouteRequestResultCode result_code);

  // Adds `route` to the list of routes. Called in the callback for
  // CreateRoute() etc. so that even if the callback is called before
  // OnRoutesUpdated(), MediaRouter is still aware of the route.
  void OnRouteAdded(mojom::MediaRouteProviderId provider_id,
                    const MediaRoute& route);

  // Converts the callback result of calling Mojo CreateRoute()/JoinRoute()
  // into a local callback.
  void RouteResponseReceived(const std::string& presentation_id,
                             mojom::MediaRouteProviderId provider_id,
                             MediaRouteResponseCallback callback,
                             bool is_join,
                             const std::optional<MediaRoute>& media_route,
                             mojom::RoutePresentationConnectionPtr connection,
                             const std::optional<std::string>& error_text,
                             mojom::RouteRequestResultCode result_code);

  void OnLocalDiscoveryPermissionRejected();

  // Callback called by MRP's BindMediaController().
  void OnMediaControllerBound(const MediaRoute::Id& route_id, bool success);

  void AddMirroringMediaControllerHost(const MediaRoute& route);

  // Initializes MRPs and adds them to `media_route_providers_`.
  void InitializeMediaRouteProviders();

  // Helper methods for InitializeMediaRouteProviders().
  void InitializeWiredDisplayMediaRouteProvider();
  void InitializeCastMediaRouteProvider();
  void InitializeDialMediaRouteProvider();

#if BUILDFLAG(IS_WIN)
  // Ensures that mDNS discovery is enabled in the Cast MRP. This can be
  // called many times but the MRPM will only be called once per registration
  // period.
  void EnsureMdnsDiscoveryEnabled();

  // Callback used to enable mDNS in the MRPM if a firewall prompt will not be
  // triggered. If a firewall prompt would be triggered, enabling mDNS won't
  // happen until the user is clearly interacting with MR.
  void OnFirewallCheckComplete(bool firewall_can_use_local_ports);
#endif

  // Gets the per-profile Cast SDK hash token used by Cast and DIAL MRPs.
  std::string GetHashToken();

  // Requests MRPs to update media sinks.
  void DiscoverSinksNow();

  // Called when the Mojo pointer for `provider_id` has a connection error.
  // Removes the pointer from `media_route_providers_`.
  void OnProviderConnectionError(mojom::MediaRouteProviderId provider_id);

  // Creates a binding between `this` and `receiver`.
  void BindToMojoReceiver(mojo::PendingReceiver<mojom::MediaRouter> receiver);

  void CreateRouteWithSelectedDesktop(
      mojom::MediaRouteProviderId provider_id,
      const std::string& sink_id,
      const std::string& presentation_id,
      const url::Origin& origin,
      content::WebContents* web_contents,
      base::TimeDelta timeout,
      mojom::MediaRouteProvider::CreateRouteCallback mr_callback,
      const std::string& err,
      content::DesktopMediaID media_id);

  content::BrowserContext* context() const { return context_; }

  // Methods for obtaining a pointer to the provider associated with the given
  // ID. They return a nullopt when such a provider is not found.
  std::optional<mojom::MediaRouteProviderId> GetProviderIdForPresentation(
      const std::string& presentation_id);
  std::optional<mojom::MediaRouteProviderId> GetProviderIdForRoute(
      const MediaRoute::Id& route_id);
  std::optional<mojom::MediaRouteProviderId> GetProviderIdForSink(
      const MediaSink::Id& sink_id);

  // Returns a pointer to the MediaSink whose ID is `sink_id`, or nullptr if not
  // found.
  const MediaSink* GetSinkById(const MediaSink::Id& sink_id) const;

  // Returns a pointer to the MediaRoute whose ID is `route_id`, or nullptr
  // if not found.
  const MediaRoute* GetRoute(const MediaRoute::Id& route_id) const;

  // Notifies any new observers of any existing cached routes.
  void NotifyNewObserversOfExistingRoutes();

  // Used by RecordPresentationRequestUrlBySink to record the possible ways a
  // Presentation URL can be used to start a presentation, both by the kind of
  // URL and the type of the sink the URL will be presented on.  "Normal"
  // (https:, file:, or chrome-extension:) URLs are typically implemented by
  // loading them into an offscreen tab for streaming, while Cast and DIAL URLs
  // are sent directly to a compatible device.
  enum class PresentationUrlBySink {
    kUnknown = 0,
    kNormalUrlToChromecast = 1,
    kNormalUrlToExtension = 2,
    kNormalUrlToWiredDisplay = 3,
    kCastUrlToChromecast = 4,
    kDialUrlToDial = 5,
    kRemotePlayback = 6,
    // Add new values immediately above this line.  Also update kMaxValue below
    // and the enum of the same name in tools/metrics/histograms/enums.xml.
    kMaxValue = kRemotePlayback,
  };

  static void RecordPresentationRequestUrlBySink(
      const MediaSource& source,
      mojom::MediaRouteProviderId provider_id);

  // Returns true when there is at least one MediaRoute that can be returned by
  // JoinRoute().
  bool HasJoinableRoute() const;

  // Returns true if the default MRPs should be initialized.
  bool ShouldInitializeMediaRouteProviders() const;

  // Disables the initialization of default MRPs for tests.
  void DisableMediaRouteProvidersForTest() {
    disable_media_route_providers_for_test_ = true;
  }

  friend class MediaRouterDesktopTest;
  friend class MediaRouterFactory;
  friend class MediaRouterMojoTest;
  friend class MediaRouterIntegrationBrowserTest;
  friend class MediaRouterNativeIntegrationBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest, JoinRouteTimedOutFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest, HandleIssue);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest, HandlePermissionIssue);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           PresentationConnectionStateChangedCallback);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           PresentationConnectionStateChangedCallbackRemoved);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           TestRecordPresentationRequestUrlBySink);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest, TestGetCurrentRoutes);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest, CreateRouteFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           CreateRouteIncognitoMismatchFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest, JoinRouteNotFoundFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest, TerminateRouteFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest, GetMediaController);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           SendSinkRequestsToMultipleProviders);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           SendRouteRequestsToMultipleProviders);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterDesktopTest,
                           GetMirroringMediaControllerHost);

  // Represents a query to the MediaRouteProviders for media sinks and caches
  // media sinks returned by MRPs. Holds observers for the query.
  class MediaSinksQuery {
   public:
    MediaSinksQuery();

    MediaSinksQuery(const MediaSinksQuery&) = delete;
    MediaSinksQuery& operator=(const MediaSinksQuery&) = delete;

    ~MediaSinksQuery();

    static MediaSource GetKey(const MediaSource::Id& source_id);
    static MediaSource GetKey(const MediaSinksObserver& observer);

    // Caches the list of sinks for the provider returned from the query.
    void SetSinksForProvider(mojom::MediaRouteProviderId provider_id,
                             const std::vector<MediaSink>& sinks);

    // Resets the internal state, including the cache for all the providers.
    void Reset();

    void AddObserver(MediaSinksObserver* observer);
    void RemoveObserver(MediaSinksObserver* observer);
    void NotifyObservers();
    bool HasObserver(MediaSinksObserver* observer) const;
    bool HasObservers() const;

    const std::vector<MediaSink>& cached_sink_list() const {
      return cached_sink_list_;
    }
    void set_origins(const std::vector<url::Origin>& origins) {
      origins_ = origins;
    }

   private:
    // Cached list of sinks for the query.
    std::vector<MediaSink> cached_sink_list_;

    // Cached list of origins for the query.
    // TODO(takumif): The list of supported origins may differ between MRPs, so
    // we need more fine-grained associations between sinks and origins.
    std::vector<url::Origin> origins_;

    base::ObserverList<MediaSinksObserver>::UncheckedAndDanglingUntriaged
        observers_;
  };

  // Represents a query to the MediaRouteProviders for media routes and caches
  // media routes returned by MRPs. Holds observers for the query.
  //
  // NOTE: If the to-do below for `providers_to_routes_` is fixed, then this
  // entire class can be replaced with a std::vector<MediaRoute> and a
  // base::ObserverList of observers.
  class MediaRoutesQuery {
   public:
    MediaRoutesQuery();

    MediaRoutesQuery(const MediaRoutesQuery&) = delete;
    MediaRoutesQuery& operator=(const MediaRoutesQuery&) = delete;

    ~MediaRoutesQuery();

    // Caches the list of routes for the provider returned from the query.
    void SetRoutesForProvider(mojom::MediaRouteProviderId provider_id,
                              const std::vector<MediaRoute>& routes);

    // Adds `route` to the list of routes managed by the provider and returns
    // true, if it hasn't been added already. Returns false otherwise.
    bool AddRouteForProvider(mojom::MediaRouteProviderId provider_id,
                             const MediaRoute& route);

    // Re-constructs `cached_route_list_` by merging route lists in
    // `providers_to_routes_`.
    void UpdateCachedRouteList();

    void AddObserver(MediaRoutesObserver* observer);
    void RemoveObserver(MediaRoutesObserver* observer);
    void NotifyObservers();
    bool HasObserver(MediaRoutesObserver* observer) const;
    bool HasObservers() const;
    void NotifyNewObserversOfExistingRoutes();

    const std::optional<std::vector<MediaRoute>>& cached_route_list() const {
      return cached_route_list_;
    }
    const base::flat_map<mojom::MediaRouteProviderId, std::vector<MediaRoute>>&
    providers_to_routes() const {
      return providers_to_routes_;
    }

   private:
    // Cached list of routes for the query.
    std::optional<std::vector<MediaRoute>> cached_route_list_;

    // Per-MRP lists of routes for the query.
    // TODO(crbug.com/1374496): Consider making MRP ID an attribute of
    // MediaRoute, so that we can simplify these into vectors.
    base::flat_map<mojom::MediaRouteProviderId, std::vector<MediaRoute>>
        providers_to_routes_;

    base::ObserverList<MediaRoutesObserver> observers_;

    // Set of new observers that need to be notified of existing routes.
    std::vector<raw_ptr<MediaRoutesObserver>> new_observers_;
  };

  // A MediaRoutesObserver that maintains state about the current set of media
  // routes.
  class InternalMediaRoutesObserver final : public MediaRoutesObserver {
   public:
    explicit InternalMediaRoutesObserver(media_router::MediaRouter* router);

    InternalMediaRoutesObserver(const InternalMediaRoutesObserver&) = delete;
    InternalMediaRoutesObserver& operator=(const InternalMediaRoutesObserver&) =
        delete;

    ~InternalMediaRoutesObserver() final;

    // MediaRoutesObserver
    void OnRoutesUpdated(const std::vector<MediaRoute>& routes) final;

    const std::vector<MediaRoute>& current_routes() const;

   private:
    std::vector<MediaRoute> current_routes_;
  };

  IssueManager issue_manager_;

  std::unique_ptr<InternalMediaRoutesObserver> internal_routes_observer_;

  base::flat_map<MediaSource::Id, std::unique_ptr<MediaSinksQuery>>
      sinks_queries_;

  // Holds observers for media route updates and a map of providers to route
  // ids.
  MediaRoutesQuery routes_query_;

  using PresentationConnectionMessageObserverList =
      base::ObserverList<PresentationConnectionMessageObserver>;
  base::flat_map<MediaRoute::Id,
                 std::unique_ptr<PresentationConnectionMessageObserverList>>
      message_observers_;

  base::flat_map<MediaRoute::Id, std::unique_ptr<MirroringMediaControllerHost>>
      mirroring_media_controller_hosts_;

  // Receivers for Mojo remotes to `this` held by media route providers.
  mojo::ReceiverSet<mojom::MediaRouter> receivers_;

  const raw_ptr<content::BrowserContext> context_;

  std::unique_ptr<DesktopMediaPickerController> desktop_picker_;

  // Collects logs from the Media Router and the native Media Route Providers.
  // TODO(crbug.com/40129011): Limit logging before Media Router usage.
  LoggerImpl logger_;

  MediaRouterDebuggerImpl media_router_debugger_;

  // Mojo remotes to media route providers. Providers are added via
  // RegisterMediaRouteProvider().
  base::flat_map<mojom::MediaRouteProviderId,
                 mojo::Remote<mojom::MediaRouteProvider>>
      media_route_providers_;

  // MediaRouteProvider for casting to local screens.
  std::unique_ptr<WiredDisplayMediaRouteProvider> wired_display_provider_;

  // MediaRouteProvider for casting to Cast devices.
  std::unique_ptr<CastMediaRouteProvider, base::OnTaskRunnerDeleter>
      cast_provider_;

  // MediaRouteProvider for DIAL.
  std::unique_ptr<DialMediaRouteProvider, base::OnTaskRunnerDeleter>
      dial_provider_;

  // May be nullptr if default Media Route Providers are disabled for
  // tests.
  raw_ptr<DualMediaSinkService> media_sink_service_{nullptr};

  base::CallbackListSubscription media_sink_service_subscription_;

  // A status object that keeps track of sinks discovered by media sink
  // services.
  MediaSinkServiceStatus media_sink_service_status_;

  // Set by tests to disable the initialization of MRPs.
  bool disable_media_route_providers_for_test_{false};

  base::WeakPtrFactory<MediaRouterDesktop> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_DESKTOP_H_
