# Device Trust Signals (`chrome/browser/enterprise/connectors/device_trust/signals`)

Wires Device Trust signal collection (`SignalsService`) for inline handshake
attestation across Desktop, ChromeOS, and Android.

## Adding or Extending Signals

- **NEVER add new `SignalsDecorator` subclasses under `decorators/`:** Under no
  circumstances should new platform or signal-specific decorators be added
  under `chrome/browser/enterprise/connectors/device_trust/signals/decorators/`
  or registered in `CreateSignalsService()` (`signals_service_factory.cc`).
  That per-decorator hierarchy is legacy.
- **Route new signals through `SignalsAggregator`:** Always collect new signals
  in a `device_signals::SignalsCollector`
  (`components/device_signals/core/browser/` or
  `chrome/browser/enterprise/signals/`, registered in
  `SignalsAggregatorFactory`) so permission checks
  (`UserPermissionService::CanCollectSignals()`) and
  `Enterprise.DeviceSignals.*` telemetry remain consistent across Device Trust
  and Chrome Enterprise reporting. Bind asynchronous collector callbacks with
  `weak_factory_.GetWeakPtr()` rather than free functions holding
  `std::ref(response)`.
  - **Desktop (Windows, macOS, Linux):** Request the `SignalName` bundle in
    `BrowserSignalsDecorator::Decorate()`
    (`decorators/browser/browser_signals_decorator.cc`, with
    `request.trigger = device_signals::Trigger::kBrowserNavigation`) and copy
    the field into the Device Trust dictionary in
    `BrowserSignalsDecorator::OnAggregatedSignalsReceived()`.
  - **Android:** Request and unpack the bundle in `SignalsAggregatorDecorator`
    (`components/enterprise/device_trust/core/signals/decorators/common/signals_aggregator_decorator.cc`).
- **Keep rollout flags independent across consumers:** When a collector in
  `SignalsAggregator` serves both Device Trust (`Trigger::kBrowserNavigation`)
  and Chrome Enterprise reporting (`Trigger::kSignalsReport`), gate collection
  and consumer exposure per `request.trigger` when each consumer has its own
  `BASE_FEATURE` rollout flag.
- **NEVER add short per-signal timeouts that report false failures:** Do not
  wrap shared browser initialization (such as `OSCryptAsync::GetInstance()`) in
  short fallback timers that map slow cold-boot startups to error states;
  `DeviceTrustNavigationThrottle` already bounds the overall handshake with
  `timeouts::kHandshakeTimeout`.

## Testing

- Update the signal contract in
  `components/device_signals/test/signals_contract.cc` whenever adding a new
  Device Trust signal key.
- Unit-test collector logic on the `SignalsCollector` itself, unit-test
  dictionary mapping in `browser_signals_decorator_unittest.cc` using
  `MockSignalsAggregator`, and cover end-to-end wiring in
  `chrome/browser/enterprise/connectors/device_trust/device_trust_browsertest.cc`.
