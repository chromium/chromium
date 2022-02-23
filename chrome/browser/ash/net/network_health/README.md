# Network Events

Notifies clients of network events. [Design Doc]

This code is maintained by the [Network Health and Configuration] team. See also
documentation for [Network Diagnostic Routines] and [Network Health telemetry].

[TOC]

## Listening to network events

Clients interested in listening to network events may implement the
`NetworkEventsObserver` Mojo interface, defined in [network_health.mojom]. The
remote end of the `NetworkEventsObserver` interface must be added as an observer
to the `NetworkHealthService` Mojo interface, also defined in
[network_health.mojom]. Chrome clients can interact with the
`NetworkHealthService` Mojo interface by using the global
[NetworkHealthService] class. Note that Chrome OS clients may listen to
network events through the [cros_healthd] daemon by [adding] a
`NetworkEventsObserver` remote to cros_healthd. TODO(khegde): Replace CL with
source once this lands.

## Available network events

See NetworkEventsObserver in [network_health.mojom] for available events.

## Demo Run

The following is an example run that uses the cros-health-tool to listen for
network events.

1. Ensure the active network is online. Then, start cros-health-tool:\
`$cros-health-tool event --category=network --length_seconds=150`

2. Disconnect the active network. Output:\
`Network event received: Connection state changed, Network guid: fake-guid, Connection state: NetworkState::kNotConnected`

3. Reconnect the active network. Output:\
`Network event received: Connection state changed, Network guid: fake-guid, Connection state: NetworkState::kConnecting`\
`Network event received: Signal strength changed, Network guid: fake-guid, Signal strength: 60`\
`Network event received: Connection state changed, Network guid: fake-guid, Connection state: NetworkState::kConnected`\
`Network event received: Connection state changed, Network guid: fake-guid, Connection state: NetworkState::kOnline`

4. Move the device to a region with weaker signal strength. Output:\
`Network event received: Signal strength changed, Network guid: fake-guid, Signal strength: 48`

[Design Doc]: https://docs.google.com/document/d/18ehcBF2iC1rZDo9AV79-qJ5KUfSGIUeqX0bLDRD3XHI/edit?usp=sharing&resourcekey=0-1mYPArwll_OTBaKgQ1qeDw
[Network Health and Configuration]: https://docs.google.com/document/d/10DSy-jZXaRo9I9aq1UqERy76t7HkgGvInWk57pHEkzg
[Network Diagnostic Routines]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/net/network_diagnostics/README.md
[Network Health telemetry]: https://source.chromium.org/chromium/chromium/src/+/main:chromeos/services/network_health/public/mojom/network_health.mojom
[network_health.mojom]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/diagnostics/mojo/network_health.mojom
[NetworkHealthService]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/net/network_health/network_health_service.h
[cros_healthd]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/diagnostics/cros_healthd/
[adding]: https://chromium-review.googlesource.com/c/chromiumos/platform2/+/2627331/8/diagnostics/mojo/cros_healthd.mojom#465

