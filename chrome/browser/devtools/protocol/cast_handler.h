// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_CAST_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_CAST_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/protocol/cast.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes_observer.h"
#include "chrome/browser/ui/media_router/query_result_manager.h"
#include "components/media_router/browser/issues_observer.h"
#include "components/media_router/common/mojom/media_router.mojom.h"

namespace content {
class WebContents;
}

namespace media_router {
class MediaRouter;
class StartPresentationContext;
}  // namespace media_router

class CastHandler : public protocol::Cast::Backend,
                    public media_router::MediaSinkWithCastModesObserver {
 public:
  CastHandler(content::WebContents* web_contents,
              protocol::UberDispatcher* dispatcher);

  CastHandler(const CastHandler&) = delete;
  CastHandler& operator=(const CastHandler&) = delete;

  ~CastHandler() override;

  // protocol::Cast::Backend:
  protocol::Response SetSinkToUse(const std::string& in_sink_name) override;
  void StartDesktopMirroring(
      const std::string& in_sink_name,
      std::unique_ptr<StartDesktopMirroringCallback> callback) override;
  void StartTabMirroring(
      const std::string& in_sink_name,
      std::unique_ptr<StartTabMirroringCallback> callback) override;
  protocol::Response StopCasting(const std::string& in_sink_name) override;
  protocol::Response Enable(
      protocol::Maybe<std::string> in_presentation_url) override;
  protocol::Response Disable() override;

  // media_router::MediaSinkWithCastModesObserver:
  void OnSinksUpdated(
      const std::vector<media_router::MediaSinkWithCastModes>& sinks) override;

 private:
  friend class CastHandlerTest;

  class MediaRoutesObserver;
  class IssuesObserver;

  // Constructor that does not wire the handler to a dispatcher. Used in tests.
  explicit CastHandler(content::WebContents* web_contents);

  // Initializes the handler if it hasn't been initialized yet. Returns
  // Response::Success() if initialization succeeds, otherwise an error.
  protocol::Response EnsureInitialized();

  void StartPresentation(
      const std::string& sink_name,
      std::unique_ptr<media_router::StartPresentationContext> context);

  // Returns an empty ID if a sink was not found.
  media_router::MediaSink::Id GetSinkIdByName(
      const std::string& sink_name) const;

  // Returns an empty ID if a route was not found.
  media_router::MediaRoute::Id GetRouteIdForSink(
      const media_router::MediaSink::Id& sink_id) const;

  void StartObservingForSinks(protocol::Maybe<std::string> presentation_url);

  // Sends a notification that sinks (or their associated routes) have been
  // updated.
  void SendSinkUpdate();

  void OnDesktopMirroringStarted(
      std::unique_ptr<StartDesktopMirroringCallback> callback,
      media_router::mojom::RoutePresentationConnectionPtr connection,
      const media_router::RouteRequestResult& result);
  void OnTabMirroringStarted(
      std::unique_ptr<StartTabMirroringCallback> callback,
      media_router::mojom::RoutePresentationConnectionPtr connection,
      const media_router::RouteRequestResult& result);
  void OnPresentationStarted(
      std::unique_ptr<media_router::StartPresentationContext> context,
      media_router::mojom::RoutePresentationConnectionPtr connection,
      const media_router::RouteRequestResult& result);
  void OnIssue(const std::string& issue);

  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<media_router::MediaRouter> router_;

  std::unique_ptr<media_router::QueryResultManager> query_result_manager_;
  std::unique_ptr<MediaRoutesObserver> routes_observer_;
  std::unique_ptr<IssuesObserver> issues_observer_;

  std::vector<media_router::MediaSinkWithCastModes> sinks_;

  // IDs of Media Routes initiated through the handler. This may contain routes
  // that have already been terminated.
  base::flat_set<media_router::MediaRoute::Id> initiated_routes_;

  std::unique_ptr<protocol::Cast::Frontend> frontend_;

  base::WeakPtrFactory<CastHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_CAST_HANDLER_H_
