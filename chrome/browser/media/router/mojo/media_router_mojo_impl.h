// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "components/media_router/browser/issue_manager.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/browser/media_router_base.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/common/issue.h"
#include "components/media_router/common/mojom/logger.mojom.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "components/media_router/common/route_request_result.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace content {
class BrowserContext;
}

namespace media {
class FlingingController;
}

namespace media_router {

enum class MediaRouteProviderWakeReason;

// MediaRouter implementation that delegates calls to a MediaRouteProvider.
class MediaRouterMojoImpl : public MediaRouterBase,
                            public mojom::MediaRouter,
                            public base::SupportsWeakPtr<MediaRouterMojoImpl> {
 public:
  MediaRouterMojoImpl(const MediaRouterMojoImpl&) = delete;
  MediaRouterMojoImpl& operator=(const MediaRouterMojoImpl&) = delete;

  ~MediaRouterMojoImpl() override;

  // MediaRouter implementation.
  void CreateRoute(const MediaSource::Id& source_id,
                   const MediaSink::Id& sink_id,
                   const url::Origin& origin,
                   content::WebContents* web_contents,
                   MediaRouteResponseCallback callback,
                   base::TimeDelta timeout,
                   bool off_the_record) final;
  void JoinRoute(const MediaSource::Id& source_id,
                 const std::string& presentation_id,
                 const url::Origin& origin,
                 content::WebContents* web_contents,
                 MediaRouteResponseCallback callback,
                 base::TimeDelta timeout,
                 bool off_the_record) final;
  void TerminateRoute(const MediaRoute::Id& route_id) final;
  void DetachRoute(MediaRoute::Id route_id) final;
  void SendRouteMessage(const MediaRoute::Id& route_id,
                        const std::string& message) final;
  void SendRouteBinaryMessage(const MediaRoute::Id& route_id,
                              std::unique_ptr<std::vector<uint8_t>> data) final;
  IssueManager* GetIssueManager() final;
  void OnUserGesture() override;
  std::vector<MediaRoute> GetCurrentRoutes() const override;
  std::unique_ptr<media::FlingingController> GetFlingingController(
      const MediaRoute::Id& route_id) override;
  void GetMediaController(
      const MediaRoute::Id& route_id,
      mojo::PendingReceiver<mojom::MediaController> controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) final;
  base::Value GetLogs() const override;
  void RegisterMediaRouteProvider(mojom::MediaRouteProviderId provider_id,
                                  mojo::PendingRemote<mojom::MediaRouteProvider>
                                      media_route_provider_remote) override;

  // Issues 0+ calls to the provider given by |provider_id| to ensure its state
  // is in sync with MediaRouter on a best-effort basis.
  virtual void SyncStateToMediaRouteProvider(
      mojom::MediaRouteProviderId provider_id);

 protected:
  // Standard constructor, used by
  // MediaRouterMojoImplFactory::GetApiForBrowserContext.
  explicit MediaRouterMojoImpl(content::BrowserContext* context);

  void Initialize() override;

  // Requests MRPs to update media sinks.
  void DiscoverSinksNow();

  // Called when the Mojo pointer for |provider_id| has a connection error.
  // Removes the pointer from |media_route_providers_|.
  void OnProviderConnectionError(mojom::MediaRouteProviderId provider_id);

  // Creates a binding between |this| and |receiver|.
  void BindToMojoReceiver(mojo::PendingReceiver<mojom::MediaRouter> receiver);

  // Methods for obtaining a pointer to the provider associated with the given
  // object. They return a nullopt when such a provider is not found.
  virtual absl::optional<mojom::MediaRouteProviderId>
  GetProviderIdForPresentation(const std::string& presentation_id);
  absl::optional<mojom::MediaRouteProviderId> GetProviderIdForRoute(
      const MediaRoute::Id& route_id);

  void CreateRouteWithSelectedDesktop(
      mojom::MediaRouteProviderId provider_id,
      const std::string& sink_id,
      const std::string& presentation_id,
      const url::Origin& origin,
      content::WebContents* web_contents,
      base::TimeDelta timeout,
      bool off_the_record,
      mojom::MediaRouteProvider::CreateRouteCallback mr_callback,
      const std::string& err,
      content::DesktopMediaID media_id);

  content::BrowserContext* context() const { return context_; }

  // mojom::MediaRouter implementation.
  void OnSinksReceived(mojom::MediaRouteProviderId provider_id,
                       const std::string& media_source,
                       const std::vector<MediaSinkInternal>& internal_sinks,
                       const std::vector<url::Origin>& origins) override;

  LoggerImpl* GetLogger() override;

  // Mojo remotes to media route providers. Providers are added via
  // RegisterMediaRouteProvider().
  base::flat_map<mojom::MediaRouteProviderId,
                 mojo::Remote<mojom::MediaRouteProvider>>
      media_route_providers_;

 private:
  friend class MediaRouterFactory;
  friend class MediaRouterMojoImplTest;
  friend class MediaRouterMojoTest;
  friend class MediaRouterIntegrationBrowserTest;
  friend class MediaRouterNativeIntegrationBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, JoinRouteTimedOutFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           JoinRouteIncognitoMismatchFails);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, HandleIssue);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallback);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           PresentationConnectionStateChangedCallbackRemoved);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest,
                           TestRecordPresentationRequestUrlBySink);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterMojoImplTest, TestGetCurrentRoutes);

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

    base::ObserverList<MediaSinksObserver>::Unchecked observers_;
  };

  // Represents a query to the MediaRouteProviders for media routes and caches
  // media routes returned by MRPs. Holds observers for the query.
  //
  // NOTE: If the to-do below for providers_to_routes_ is fixed, then this
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

    // Adds |route| to the list of routes managed by the provider and returns
    // true, if it hasn't been added already. Returns false otherwise.
    bool AddRouteForProvider(mojom::MediaRouteProviderId provider_id,
                             const MediaRoute& route);

    // Re-constructs |cached_route_list_| by merging route lists in
    // |providers_to_routes_|.
    void UpdateCachedRouteList();

    void AddObserver(MediaRoutesObserver* observer);
    void RemoveObserver(MediaRoutesObserver* observer);
    void NotifyObservers();
    bool HasObserver(MediaRoutesObserver* observer) const;
    bool HasObservers() const;

    const absl::optional<std::vector<MediaRoute>>& cached_route_list() const {
      return cached_route_list_;
    }
    const base::flat_map<mojom::MediaRouteProviderId, std::vector<MediaRoute>>&
    providers_to_routes() const {
      return providers_to_routes_;
    }

   private:
    // Cached list of routes for the query.
    absl::optional<std::vector<MediaRoute>> cached_route_list_;

    // Per-MRP lists of routes for the query.
    // TODO(crbug.com/1374496): Consider making MRP ID an attribute of
    // MediaRoute, so that we can simplify these into vectors.
    base::flat_map<mojom::MediaRouteProviderId, std::vector<MediaRoute>>
        providers_to_routes_;

    base::ObserverList<MediaRoutesObserver> observers_;
  };

  // See note in OnDesktopPickerDone().
  struct PendingStreamRequest {
    std::string stream_id;
    int render_process_id;
    int render_frame_id;
    url::Origin origin;
  };

  // A MediaRoutesObserver that maintains state about the current set of media
  // routes.
  class InternalMediaRoutesObserver : public MediaRoutesObserver {
   public:
    explicit InternalMediaRoutesObserver(media_router::MediaRouter* router);

    InternalMediaRoutesObserver(const InternalMediaRoutesObserver&) = delete;
    InternalMediaRoutesObserver& operator=(const InternalMediaRoutesObserver&) =
        delete;

    ~InternalMediaRoutesObserver() override;

    // MediaRoutesObserver
    void OnRoutesUpdated(const std::vector<MediaRoute>& routes) override;

    const std::vector<MediaRoute>& current_routes() const;

   private:
    std::vector<MediaRoute> current_routes_;
  };

  // MediaRouter implementation.
  bool RegisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void UnregisterMediaSinksObserver(MediaSinksObserver* observer) override;
  void RegisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void UnregisterMediaRoutesObserver(MediaRoutesObserver* observer) override;
  void RegisterPresentationConnectionMessageObserver(
      PresentationConnectionMessageObserver* observer) override;
  void UnregisterPresentationConnectionMessageObserver(
      PresentationConnectionMessageObserver* observer) override;

  // Notifies |observer| of any existing cached routes, if it is still
  // registered.
  void NotifyOfExistingRoutes(
      base::WeakPtr<MediaRoutesObserver> observer) const;

  // mojom::MediaRouter implementation.
  void OnIssue(const IssueInfo& issue) override;
  void ClearTopIssueForSink(const MediaSink::Id& sink_id) override;
  void OnRoutesUpdated(mojom::MediaRouteProviderId provider_id,
                       const std::vector<MediaRoute>& routes) override;
  void OnPresentationConnectionStateChanged(
      const std::string& route_id,
      blink::mojom::PresentationConnectionState state) override;
  void OnPresentationConnectionClosed(
      const std::string& route_id,
      blink::mojom::PresentationConnectionCloseReason reason,
      const std::string& message) override;
  void OnRouteMessagesReceived(
      const std::string& route_id,
      std::vector<mojom::RouteMessagePtr> messages) override;
  void GetLogger(mojo::PendingReceiver<mojom::Logger> receiver) override;
  void GetLogsAsString(GetLogsAsStringCallback callback) override;
  void GetMediaSinkServiceStatus(
      mojom::MediaRouter::GetMediaSinkServiceStatusCallback callback) override;
  void GetMirroringServiceHostForTab(
      int32_t frame_tree_node_id,
      mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost> receiver)
      override;
  void GetMirroringServiceHostForDesktop(
      const std::string& desktop_stream_id,
      mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost> receiver)
      override;
  void GetMirroringServiceHostForOffscreenTab(
      const GURL& presentation_url,
      const std::string& presentation_id,
      mojo::PendingReceiver<mirroring::mojom::MirroringServiceHost> receiver)
      override;

  // Result callback when Mojo TerminateRoute is invoked.
  // |route_id|: ID of MediaRoute passed to the TerminateRoute request.
  // |provider_id|: ID of MediaRouteProvider that handled the request.
  // |error_text|: Error message if an error occurred.
  // |result_code|: The result of the request.
  void OnTerminateRouteResult(const MediaRoute::Id& route_id,
                              mojom::MediaRouteProviderId provider_id,
                              const absl::optional<std::string>& error_text,
                              mojom::RouteRequestResultCode result_code);

  // Adds |route| to the list of routes. Called in the callback for
  // CreateRoute() etc. so that even if the callback is called before
  // OnRoutesUpdated(), MediaRouter is still aware of the route.
  void OnRouteAdded(mojom::MediaRouteProviderId provider_id,
                    const MediaRoute& route);

  // Converts the callback result of calling Mojo CreateRoute()/JoinRoute()
  // into a local callback.
  void RouteResponseReceived(const std::string& presentation_id,
                             mojom::MediaRouteProviderId provider_id,
                             bool is_off_the_record,
                             MediaRouteResponseCallback callback,
                             bool is_join,
                             const absl::optional<MediaRoute>& media_route,
                             mojom::RoutePresentationConnectionPtr connection,
                             const absl::optional<std::string>& error_text,
                             mojom::RouteRequestResultCode result_code);

  // Callback called by MRP's CreateMediaRouteController().
  void OnMediaControllerCreated(const MediaRoute::Id& route_id, bool success);

  // Method for obtaining a pointer to the provider associated with the given
  // object. Returns a nullopt when such a provider is not found.
  absl::optional<mojom::MediaRouteProviderId> GetProviderIdForSink(
      const MediaSink::Id& sink_id);

  // Gets the sink with the given ID from lists of sinks held by sink queries.
  // Returns a nullptr if none is found.
  const MediaSink* GetSinkById(const MediaSink::Id& sink_id) const;

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
    // Add new values immediately above this line.  Also update kMaxValue below
    // and the enum of the same name in tools/metrics/histograms/enums.xml.
    kMaxValue = kDialUrlToDial,
  };

  static void RecordPresentationRequestUrlBySink(
      const MediaSource& source,
      mojom::MediaRouteProviderId provider_id);

  // Returns true when there is at least one MediaRoute that can be returned by
  // JoinRoute().
  bool HasJoinableRoute() const;

  // Returns a pointer to the MediaRoute whose ID is |route_id|, or nullptr
  // if not found.
  const MediaRoute* GetRoute(const MediaRoute::Id& route_id) const;

  // KeyedService:
  void Shutdown() override;

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

  // Receivers for Mojo remotes to |this| held by media route providers.
  mojo::ReceiverSet<mojom::MediaRouter> receivers_;

  const raw_ptr<content::BrowserContext> context_;

  DesktopMediaPickerController desktop_picker_;

  absl::optional<PendingStreamRequest> pending_stream_request_;

  // Collects logs from the Media Router and the native Media Route Providers.
  // TODO(crbug.com/1077138): Limit logging before Media Router usage.
  LoggerImpl logger_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MOJO_MEDIA_ROUTER_MOJO_IMPL_H_
