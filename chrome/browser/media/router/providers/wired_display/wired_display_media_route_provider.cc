// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_presentation_receiver_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/route_request_result.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

using display::Display;

namespace media_router {

namespace {

MediaSinkInternal CreateSinkForDisplay(const Display& display,
                                       int display_index) {
  const std::string sink_id =
      WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(display);
  const std::string sink_name =
      l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_WIRED_DISPLAY_SINK_NAME,
                                base::FormatNumber(display_index));
  MediaSink sink(sink_id, sink_name, SinkIconType::WIRED_DISPLAY,
                 mojom::MediaRouteProviderId::WIRED_DISPLAY);
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
  if (display2.id() == primary_id) {
    return false;
  }
  if (display1.id() == primary_id) {
    return true;
  }
  return display1.bounds().y() < display2.bounds().y() ||
         (display1.bounds().y() == display2.bounds().y() &&
          display1.bounds().x() < display2.bounds().x());
}

}  // namespace

// static
const mojom::MediaRouteProviderId WiredDisplayMediaRouteProvider::kProviderId =
    mojom::MediaRouteProviderId::WIRED_DISPLAY;

// static
std::string WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(
    const Display& display) {
  return "wired_display_" + base::NumberToString(display.id());
}

// static
std::string WiredDisplayMediaRouteProvider::GetRouteDescription(
    const std::string& media_source) {
  return l10n_util::GetStringFUTF8(
      IDS_MEDIA_ROUTER_PRESENTATION_ROUTE_DESCRIPTION,
      base::UTF8ToUTF16(url::Origin::Create(GURL(media_source)).host()));
}

WiredDisplayMediaRouteProvider::WiredDisplayMediaRouteProvider(
    mojo::PendingReceiver<mojom::MediaRouteProvider> receiver,
    mojo::PendingRemote<mojom::MediaRouter> media_router,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      media_router_(std::move(media_router)),
      profile_(profile) {}

WiredDisplayMediaRouteProvider::~WiredDisplayMediaRouteProvider() = default;

void WiredDisplayMediaRouteProvider::CreateRoute(
    const std::string& media_source,
    const std::string& sink_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t frame_tree_node_id,
    base::TimeDelta timeout,
    CreateRouteCallback callback) {
  DCHECK(!base::Contains(presentations_, presentation_id));
  std::optional<Display> display = GetDisplayBySinkId(sink_id);
  if (!display) {
    std::move(callback).Run(std::nullopt, nullptr,
                            std::string("Display not found"),
                            mojom::RouteRequestResultCode::SINK_NOT_FOUND);
    return;
  }

  // If there already is a presentation on |display|, terminate it.
  TerminatePresentationsOnDisplay(*display);
  // Use |presentation_id| as the route ID. This MRP creates only one route per
  // presentation ID.
  MediaRoute route(presentation_id, MediaSource(media_source), sink_id,
                   GetRouteDescription(media_source), true);
  route.set_local_presentation(true);
  route.set_controller_type(RouteControllerType::kGeneric);

  Presentation& presentation =
      presentations_.emplace(presentation_id, route).first->second;
  presentation.set_receiver(
      CreatePresentationReceiver(presentation_id, &presentation, *display));
  presentation.receiver()->Start(presentation_id, GURL(media_source));
  std::move(callback).Run(route, nullptr, std::nullopt,
                          mojom::RouteRequestResultCode::OK);
  NotifyRouteObservers();
}

void WiredDisplayMediaRouteProvider::JoinRoute(
    const std::string& media_source,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t frame_tree_node_id,
    base::TimeDelta timeout,
    JoinRouteCallback callback) {
  std::move(callback).Run(
      std::nullopt, nullptr,
      std::string("Join should be handled by the presentation manager"),
      mojom::RouteRequestResultCode::UNKNOWN_ERROR);
}

void WiredDisplayMediaRouteProvider::TerminateRoute(
    const std::string& route_id,
    TerminateRouteCallback callback) {
  auto it = presentations_.find(route_id);
  if (it == presentations_.end()) {
    std::move(callback).Run(std::string("Presentation not found"),
                            mojom::RouteRequestResultCode::ROUTE_NOT_FOUND);
    return;
  }

  // The presentation will be removed from |presentations_| in the termination
  // callback of its receiver.
  it->second.receiver()->Terminate();
  std::move(callback).Run(std::nullopt, mojom::RouteRequestResultCode::OK);
}

void WiredDisplayMediaRouteProvider::SendRouteMessage(
    const std::string& media_route_id,
    const std::string& message) {
  // Messages should be handled by LocalPresentationManager.
  NOTREACHED_IN_MIGRATION();
}

void WiredDisplayMediaRouteProvider::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data) {
  // Messages should be handled by LocalPresentationManager.
  NOTREACHED_IN_MIGRATION();
}

void WiredDisplayMediaRouteProvider::StartObservingMediaSinks(
    const std::string& media_source) {
  if (!IsValidStandardPresentationSource(media_source))
    return;

  // Start observing displays if |this| isn't already observing.
  if (!display_observer_)
    display_observer_.emplace(this);
  sink_queries_.insert(media_source);
  DiscoverSinksNow();
}

void WiredDisplayMediaRouteProvider::StopObservingMediaSinks(
    const std::string& media_source) {
  sink_queries_.erase(media_source);
}

void WiredDisplayMediaRouteProvider::StartObservingMediaRoutes() {
  std::vector<MediaRoute> route_list;
  for (const auto& presentation : presentations_)
    route_list.push_back(presentation.second.route());

  media_router_->OnRoutesUpdated(kProviderId, route_list);
}

void WiredDisplayMediaRouteProvider::DetachRoute(const std::string& route_id) {
  // Detaching should be handled by LocalPresentationManager.
  NOTREACHED_IN_MIGRATION();
}

void WiredDisplayMediaRouteProvider::EnableMdnsDiscovery() {}

void WiredDisplayMediaRouteProvider::DiscoverSinksNow() {
  NotifySinkObservers();
}

void WiredDisplayMediaRouteProvider::BindMediaController(
    const std::string& route_id,
    mojo::PendingReceiver<mojom::MediaController> media_controller,
    mojo::PendingRemote<mojom::MediaStatusObserver> observer,
    BindMediaControllerCallback callback) {
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

void WiredDisplayMediaRouteProvider::GetState(GetStateCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(mojom::ProviderStatePtr());
}

void WiredDisplayMediaRouteProvider::OnDisplayAdded(
    const Display& new_display) {
  NotifySinkObservers();
}

void WiredDisplayMediaRouteProvider::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  for (const auto& display : removed_displays) {
    const std::string sink_id =
        WiredDisplayMediaRouteProvider::GetSinkIdForDisplay(display);
    auto it =
        base::ranges::find(presentations_, sink_id,
                           [](const Presentations::value_type& presentation) {
                             return presentation.second.route().media_sink_id();
                           });
    if (it != presentations_.end()) {
      it->second.receiver()->ExitFullscreen();
    }
  }
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
    : route_(route), status_(std::in_place) {}

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

  media_router_->OnRoutesUpdated(kProviderId, route_list);
}

void WiredDisplayMediaRouteProvider::NotifySinkObservers() {
  std::vector<MediaSinkInternal> sinks = GetSinks();
  device_count_metrics_.RecordDeviceCountsIfNeeded(sinks.size(), sinks.size());
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
  std::erase_if(displays, [&primary_display](const Display& display) {
    return display.id() != primary_display.id() &&
           display.bounds() == primary_display.bounds();
  });
  // If all the displays are mirrored, the user should not be able to present to
  // them.
  return displays.size() == 1 ? std::vector<Display>() : displays;
}

void WiredDisplayMediaRouteProvider::RemovePresentationById(
    const std::string& presentation_id) {
  auto entry = presentations_.find(presentation_id);
  if (entry == presentations_.end())
    return;
  media_router_->OnPresentationConnectionStateChanged(
      entry->second.route().media_route_id(),
      blink::mojom::PresentationConnectionState::TERMINATED);
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

std::optional<Display> WiredDisplayMediaRouteProvider::GetDisplayBySinkId(
    const std::string& sink_id) const {
  std::vector<Display> displays = GetAllDisplays();
  auto it = base::ranges::find(displays, sink_id, &GetSinkIdForDisplay);
  return it == displays.end() ? std::nullopt
                              : std::make_optional<Display>(std::move(*it));
}

}  // namespace media_router
