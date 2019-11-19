// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_presentation_receiver_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

using display::Display;

namespace media_router {

namespace {

bool IsPresentationSource(const std::string& media_source) {
  const GURL source_url(media_source);
  return source_url.is_valid() && source_url.SchemeIsHTTPOrHTTPS() &&
         !base::StartsWith(source_url.spec(), kLegacyCastPresentationUrlPrefix,
                           base::CompareCase::INSENSITIVE_ASCII);
}

MediaSinkInternal CreateSinkForDisplay(const Display& display,
                                       int display_index) {
  const std::string sink_id =
      WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(display);
  const std::string sink_name =
      l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_WIRED_DISPLAY_SINK_NAME,
                                base::FormatNumber(display_index));
  MediaSink sink(sink_id, sink_name, SinkIconType::WIRED_DISPLAY,
                 MediaRouteProviderId::WIRED_DISPLAY);
  MediaSinkInternal sink_internal;
  sink_internal.set_sink(sink);
  return sink_internal;
}

// Returns true if |display1| should come before |display2| when displays are
// sorted. Primary displays and displays to the top-left take priority, in
// that order.
bool CompareDisplays(int64_t primary_id,
                     const Display& display1,
                     const Display& display2) {
  if (display1.id() == primary_id)
    return true;
  if (display2.id() == primary_id)
    return false;
  return display1.bounds().y() < display2.bounds().y() ||
         (display1.bounds().y() == display2.bounds().y() &&
          display1.bounds().x() < display2.bounds().x());
}

}  // namespace

// static
const MediaRouteProviderId WiredDisplayMediaRouteProvider::kProviderId =
    MediaRouteProviderId::WIRED_DISPLAY;

// static
std::string WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(
    const Display& display) {
  return "wired_display_" + std::to_string(display.id());
}

// static
std::string WiredDisplayMediaRouteProvider::GetRouteDescription(
    const std::string& media_source) {
  return l10n_util::GetStringFUTF8(
      IDS_MEDIA_ROUTER_WIRED_DISPLAY_ROUTE_DESCRIPTION,
      base::UTF8ToUTF16(url::Origin::Create(GURL(media_source)).host()));
}

WiredDisplayMediaRouteProvider::WiredDisplayMediaRouteProvider(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
    mojo::PendingRemote<mojom::MediaRouter> media_router,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      media_router_(std::move(media_router)),
      profile_(profile) {
  media_router_->OnSinkAvailabilityUpdated(
      kProviderId, mojom::MediaRouter::SinkAvailability::PER_SOURCE);
}

WiredDisplayMediaRouteProvider::~WiredDisplayMediaRouteProvider() {
  if (is_observing_displays_) {
    display::Screen::GetScreen()->RemoveObserver(this);
    is_observing_displays_ = false;
  }
}

void WiredDisplayMediaRouteProvider::CreateRoute(
    const std::string& media_source,
    const std::string& sink_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    CreateRouteCallback callback) {
  DCHECK(!base::Contains(presentations_, presentation_id));
  base::Optional<Display> display = GetDisplayBySinkId(sink_id);
  if (!display) {
    std::move(callback).Run(base::nullopt, nullptr,
                            std::string("Display not found"),
                            RouteRequestResult::SINK_NOT_FOUND);
    return;
  }

  // If there already is a presentation on |display|, terminate it.
  TerminatePresentationsOnDisplay(*display);
  // Use |presentation_id| as the route ID. This MRP creates only one route per
  // presentation ID.
  MediaRoute route(presentation_id, MediaSource(media_source), sink_id,
                   GetRouteDescription(media_source), true, true);
  route.set_local_presentation(true);
  route.set_incognito(profile_->IsOffTheRecord());
  route.set_controller_type(RouteControllerType::kGeneric);

  Presentation& presentation =
      presentations_.emplace(presentation_id, route).first->second;
  presentation.set_receiver(
      CreatePresentationReceiver(presentation_id, &presentation, *display));
  presentation.receiver()->Start(presentation_id, GURL(media_source));
  std::move(callback).Run(route, nullptr, base::nullopt,
                          RouteRequestResult::OK);
  NotifyRouteObservers();
}

void WiredDisplayMediaRouteProvider::JoinRoute(
    const std::string& media_source,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    JoinRouteCallback callback) {
  std::move(callback).Run(
      base::nullopt, nullptr,
      std::string("Join should be handled by the presentation manager"),
      RouteRequestResult::UNKNOWN_ERROR);
}

void WiredDisplayMediaRouteProvider::ConnectRouteByRouteId(
    const std::string& media_source,
    const std::string& route_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    ConnectRouteByRouteIdCallback callback) {
  std::move(callback).Run(
      base::nullopt, nullptr,
      std::string("Connect should be handled by the presentation manager"),
      RouteRequestResult::UNKNOWN_ERROR);
}

void WiredDisplayMediaRouteProvider::TerminateRoute(
    const std::string& route_id,
    TerminateRouteCallback callback) {
  auto it = presentations_.find(route_id);
  if (it == presentations_.end()) {
    std::move(callback).Run(std::string("Presentation not found"),
                            RouteRequestResult::ROUTE_NOT_FOUND);
    return;
  }

  // The presentation will be removed from |presentations_| in the termination
  // callback of its receiver.
  it->second.receiver()->Terminate();
  std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
}

void WiredDisplayMediaRouteProvider::SendRouteMessage(
    const std::string& media_route_id,
    const std::string& message) {
  // Messages should be handled by LocalPresentationManager.
  NOTREACHED();
}

void WiredDisplayMediaRouteProvider::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data) {
  // Messages should be handled by LocalPresentationManager.
  NOTREACHED();
}

void WiredDisplayMediaRouteProvider::StartObservingMediaSinks(
    const std::string& media_source) {
  if (!IsPresentationSource(media_source))
    return;

  // Start observing displays if |this| isn't already observing.
  if (!is_observing_displays_) {
    display::Screen::GetScreen()->AddObserver(this);
    is_observing_displays_ = true;
  }
  sink_queries_.insert(media_source);
  UpdateMediaSinks(media_source);
}

void WiredDisplayMediaRouteProvider::StopObservingMediaSinks(
    const std::string& media_source) {
  sink_queries_.erase(media_source);
}

void WiredDisplayMediaRouteProvider::StartObservingMediaRoutes(
    const std::string& media_source) {
  route_queries_.insert(media_source);
  std::vector<MediaRoute> route_list;
  for (const auto& presentation : presentations_)
    route_list.push_back(presentation.second.route());
  media_router_->OnRoutesUpdated(kProviderId, route_list, media_source, {});
}

void WiredDisplayMediaRouteProvider::StopObservingMediaRoutes(
    const std::string& media_source) {
  route_queries_.erase(media_source);
}

void WiredDisplayMediaRouteProvider::StartListeningForRouteMessages(
    const std::string& route_id) {
  // Messages should be handled by LocalPresentationManager.
}

void WiredDisplayMediaRouteProvider::StopListeningForRouteMessages(
    const std::string& route_id) {
  // Messages should be handled by LocalPresentationManager.
}

void WiredDisplayMediaRouteProvider::DetachRoute(const std::string& route_id) {
  // Detaching should be handled by LocalPresentationManager.
  NOTREACHED();
}

void WiredDisplayMediaRouteProvider::EnableMdnsDiscovery() {}

void WiredDisplayMediaRouteProvider::UpdateMediaSinks(
    const std::string& media_source) {
  if (IsPresentationSource(media_source))
    media_router_->OnSinksReceived(kProviderId, media_source, GetSinks(), {});
}

void WiredDisplayMediaRouteProvider::SearchSinks(
    const std::string& sink_id,
    const std::string& media_source,
    mojom::SinkSearchCriteriaPtr search_criteria,
    SearchSinksCallback callback) {
  // The use of this method is not required by this MRP.
  std::move(callback).Run("");
}

void WiredDisplayMediaRouteProvider::ProvideSinks(
    const std::string& provider_name,
    const std::vector<media_router::MediaSinkInternal>& sinks) {
  NOTREACHED();
}

void WiredDisplayMediaRouteProvider::CreateMediaRouteController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    CreateMediaRouteControllerCallback callback) {
  // Local screens do not support media controls.
  auto it = presentations_.find(route_id);
  if (it == presentations_.end()) {
    std::move(callback).Run(false);
    return;
  }
  it->second.SetMojoConnections(std::move(media_controller),
                                std::move(observer));
  std::move(callback).Run(true);
}

void WiredDisplayMediaRouteProvider::OnDidProcessDisplayChanges() {
  NotifySinkObservers();
}

void WiredDisplayMediaRouteProvider::OnDisplayAdded(
    const Display& new_display) {
  NotifySinkObservers();
}

void WiredDisplayMediaRouteProvider::OnDisplayRemoved(
    const Display& old_display) {
  const std::string sink_id =
      WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(old_display);
  auto it = std::find_if(
      presentations_.begin(), presentations_.end(),
      [&sink_id](
          const std::pair<const std::string, Presentation>& presentation) {
        return presentation.second.route().media_sink_id() == sink_id;
      });
  if (it != presentations_.end())
    it->second.receiver()->ExitFullscreen();
  NotifySinkObservers();
}

void WiredDisplayMediaRouteProvider::OnDisplayMetricsChanged(
    const Display& display,
    uint32_t changed_metrics) {
  NotifySinkObservers();
}

std::vector<Display> WiredDisplayMediaRouteProvider::GetAllDisplays() const {
  return display::Screen::GetScreen()->GetAllDisplays();
}

Display WiredDisplayMediaRouteProvider::GetPrimaryDisplay() const {
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}

WiredDisplayMediaRouteProvider::Presentation::Presentation(
    const MediaRoute& route)
    : route_(route), status_(base::in_place) {}

WiredDisplayMediaRouteProvider::Presentation::Presentation(
    Presentation&& other) = default;

WiredDisplayMediaRouteProvider::Presentation::~Presentation() = default;

void WiredDisplayMediaRouteProvider::Presentation::UpdatePresentationTitle(
    const std::string& title) {
  if (status_->title == title)
    return;

  status_->title = title;
  if (media_status_observer_)
    media_status_observer_->OnMediaStatusUpdated(status_.Clone());
}

void WiredDisplayMediaRouteProvider::Presentation::SetMojoConnections(
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer) {
  // This provider does not support media controls, so we do not bind
  // |media_controller| to a controller implementation.
  media_controller_receiver_ = std::move(media_controller);

  media_status_observer_.reset();
  media_status_observer_.Bind(std::move(observer));
  media_status_observer_->OnMediaStatusUpdated(status_.Clone());
  media_status_observer_.set_disconnect_handler(base::BindOnce(
      &WiredDisplayMediaRouteProvider::Presentation::ResetMojoConnections,
      base::Unretained(this)));
}

void WiredDisplayMediaRouteProvider::Presentation::ResetMojoConnections() {
  media_controller_receiver_.reset();
  media_status_observer_.reset();
}

void WiredDisplayMediaRouteProvider::NotifyRouteObservers() const {
  std::vector<MediaRoute> route_list;
  for (const auto& presentation : presentations_)
    route_list.push_back(presentation.second.route());
  for (const auto& route_query : route_queries_)
    media_router_->OnRoutesUpdated(kProviderId, route_list, route_query, {});
}

void WiredDisplayMediaRouteProvider::NotifySinkObservers() {
  std::vector<MediaSinkInternal> sinks = GetSinks();
  device_count_metrics_.RecordDeviceCountsIfNeeded(sinks.size(), sinks.size());
  ReportSinkAvailability(sinks);
  for (const auto& sink_query : sink_queries_)
    media_router_->OnSinksReceived(kProviderId, sink_query, sinks, {});
}

std::vector<MediaSinkInternal> WiredDisplayMediaRouteProvider::GetSinks()
    const {
  std::vector<MediaSinkInternal> sinks;
  std::vector<Display> displays = GetAvailableDisplays();
  for (size_t i = 0; i < displays.size(); i++)
    sinks.push_back(CreateSinkForDisplay(displays[i], i + 1));
  return sinks;
}

std::vector<Display> WiredDisplayMediaRouteProvider::GetAvailableDisplays()
    const {
  std::vector<Display> displays = GetAllDisplays();
  // If there is only one display, the user should not be able to present to it.
  // If there are no displays, GetPrimaryDisplay() below fails.
  if (displays.size() <= 1)
    return std::vector<Display>();

  const Display primary_display = GetPrimaryDisplay();
  std::sort(
      displays.begin(), displays.end(),
      [&primary_display](const Display& display1, const Display& display2) {
        return CompareDisplays(primary_display.id(), display1, display2);
      });

  // Remove displays that mirror the primary display. On some platforms such as
  // Windows, mirrored displays are reported as one display. On others, mirrored
  // displays are reported separately but with the same bounds.
  base::EraseIf(displays, [&primary_display](const Display& display) {
    return display.id() != primary_display.id() &&
           display.bounds() == primary_display.bounds();
  });
  // If all the displays are mirrored, the user should not be able to present to
  // them.
  return displays.size() == 1 ? std::vector<Display>() : displays;
}

void WiredDisplayMediaRouteProvider::ReportSinkAvailability(
    const std::vector<MediaSinkInternal>& sinks) {
  mojom::MediaRouter::SinkAvailability sink_availability =
      sinks.empty() ? mojom::MediaRouter::SinkAvailability::UNAVAILABLE
                    : mojom::MediaRouter::SinkAvailability::PER_SOURCE;
  media_router_->OnSinkAvailabilityUpdated(kProviderId, sink_availability);
}

void WiredDisplayMediaRouteProvider::RemovePresentationById(
    const std::string& presentation_id) {
  auto entry = presentations_.find(presentation_id);
  if (entry == presentations_.end())
    return;
  media_router_->OnPresentationConnectionStateChanged(
      entry->second.route().media_route_id(),
      mojom::MediaRouter::PresentationConnectionState::TERMINATED);
  presentations_.erase(entry);
  NotifyRouteObservers();
}

std::unique_ptr<WiredDisplayPresentationReceiver>
WiredDisplayMediaRouteProvider::CreatePresentationReceiver(
    const std::string& presentation_id,
    Presentation* presentation,
    const Display& display) {
  return WiredDisplayPresentationReceiverFactory::Create(
      profile_, display.bounds(),
      base::BindOnce(&WiredDisplayMediaRouteProvider::RemovePresentationById,
                     base::Unretained(this), presentation_id),
      base::BindRepeating(&WiredDisplayMediaRouteProvider::Presentation::
                              UpdatePresentationTitle,
                          base::Unretained(presentation)));
}

void WiredDisplayMediaRouteProvider::TerminatePresentationsOnDisplay(
    const display::Display& display) {
  std::vector<WiredDisplayPresentationReceiver*> presentations_to_terminate;
  // We cannot call Terminate() on the receiver while iterating over
  // |presentations_| because that might invoke a callback to delete the
  // presentation from |presentations_|.
  for (const auto& presentation : presentations_) {
    if (presentation.second.route().media_sink_id() ==
        GetSinkIdForDisplay(display)) {
      presentations_to_terminate.push_back(presentation.second.receiver());
    }
  }
  for (auto* presentation_to_terminate : presentations_to_terminate)
    presentation_to_terminate->Terminate();
}

base::Optional<Display> WiredDisplayMediaRouteProvider::GetDisplayBySinkId(
    const std::string& sink_id) const {
  std::vector<Display> displays = GetAllDisplays();
  auto it = std::find_if(displays.begin(), displays.end(),
                         [&sink_id](const Display& display) {
                           return GetSinkIdForDisplay(display) == sink_id;
                         });
  return it == displays.end() ? base::nullopt
                              : base::make_optional<Display>(std::move(*it));
}

}  // namespace media_router
