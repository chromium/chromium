# Network Diagnostics Routines

Routines for diagnosing network connectivity issues. This code is maintained by
the [Network Health and Configuration] team. [Design Doc].

[TOC]

## Using the Network Diagnostics API

Network diagnostics routines are triggered via the `NetworkDiagnosticsRoutines`
interface located in [network_diagnostics.mojom]. The interface is currently
being used by cros_healthd, chrome://network UI, and feedback reports. In order
to run a routine and view the results, a service must first acquire a
`NetworkDiagnosticsRoutines` Mojo remote from the [NetworkHealthService]. Use
`GetDiagnosticsRemoteAndBindReceiver()`.

## Adding a network diagnostics routine

To add a network diagnostics routine:
1. Expose the method to run the routine in network_diagnostics.mojom.
2. Add the implementation and unit tests [here].

Note: Any changes made to network_diagnostics.mojom must be kept in sync with
the copy in the Chromium OS repo:
[src/platform2/diagnostics/mojo/network_diagnostics.mojom].

## Understanding a routine's results

After a routine has completed running, it provides:
1. A [RoutineVerdict].
1. A list of routine specific problems detected.
    * If a routine does not run, the associated list of problems is empty.
1. A timestamp of when the routine completed.

## Breaking down the routines by connectivity level

Each routine assess the network connectivity at one of the following levels:
Local Network, DNS, Captive Portal, Firewall, and Google Services.

### Local Network Routines

Local Network routines ensure that devices are successfully and securely
connected to a router.

#### LanConnectivity

Tests whether the device is connected to a Local Area Network (LAN).

Problems: N/A

#### SignalStrength

Tests whether there is an acceptable signal strength on wireless networks.

Problems:
* `kWeakSignal`: Weak signal detected.

#### GatewayCanBePinged

Tests whether the gateway of connected networks is pingable.

Problems:
* `kUnreachableGateway`: All gateways are unreachable, hence cannot be pinged.
* `kFailedToPingDefaultNetwork`: The default network cannot be pinged.
* `kDefaultNetworkAboveLatencyThreshold`: The default network has a latency
   above the threshold.
* `kUnsuccessfulNonDefaultNetworksPings`: One or more of the non-default
   networks has failed pings.
* `kNonDefaultNetworksAboveLatencyThreshold`: One or more of the non-default
   networks has a latency above the threshold.

#### HasSecureWiFiConnection

Tests whether the WiFi connection is secure. Note that if WiFi is not connected,
the routine will not run and result in a kNotRun[code] RoutineVerdict.

Problems:
* `kSecurityTypeNone`: No security type found.
* `kSecurityTypeWep8021x`: Insecure security type Wep8021x found.
* `kSecurityTypeWepPsk`: Insecure security type WepPsk found.
* `kUnknownSecurityType`: Unknown security type found.

### DNS Routines

DNS routines ensure that the network has configured nameservers that can
successfully resolve hosts.

#### DnsResolverPresent

Tests whether a DNS resolver is available to the browser.

Problems:
* `kNoNameServersFound`: IP config has an empty or default list of name servers available.
* `kMalformedNameServers`: IP config has a list of at least one malformed name
   server.
#### DnsLatency

Tests whether the DNS latency is below an acceptable threshold.

Problems:
* `kHostResolutionFailure`: Failed to resolve one or more hosts.
* `kSlightlyAboveThreshold`: Average DNS latency across hosts is slightly above
   expected threshold.
* `kSignificantlyAboveThreshold`: Average DNS latency across hosts is
   significantly above expected threshold.

#### DnsResolution

Tests whether a DNS resolution can be completed successfully.

Problems:
* `kFailedToResolveHost`: Failed to resolve host.

### Captive Portal Routines

Captive Portal routines ensure that the active network is neither trapped behind
a captive portal nor has restricted connectivity.

#### CaptivePortal

Tests whether the internet connection is behind a captive portal.

Problems:
* `kNoActiveNetworks`: No active networks found.
* `kRestrictedConnectivity`: The active network is behind a captive portal and
    has restricted connectivity.
* `kUnknownPortalState`: The active network is not connected or the portal
    state is not available.
* `kPortalSuspected`: A portal is suspected but no redirect was provided.
* `kPortal`: The network is in a portal state with a redirect URL.
* `kProxyAuthRequired`: A proxy requiring authentication is detected.
* `kNoInternet`: The active network is connected but no internet is available
    and no proxy was detected.


### Firewall Routines

Firewall routines ensure that internet connectivity isn’t being blocked by a firewall.

#### HttpFirewall

Tests whether a firewall is blocking HTTP port 80.

Problems:
* `kDnsResolutionFailuresAboveThreshold`: DNS resolution failures above
   threshold.
* `kFirewallDetected`: Firewall detected.
* `kPotentialFirewall`: A firewall may potentially exist.

#### HttpsFirewall

Tests whether a firewall is blocking HTTPS port 443.

Problems:
* `kHighDnsResolutionFailureRate`: DNS resolution failure rate is high.
* `kFirewallDetected`: Firewall detected.
* `kPotentialFirewall`: A firewall may potentially exist.

### Google Services Routines

Tests successful communication with various Google domains.

#### HttpsLatency

Tests whether the HTTPS latency is below an acceptable threshold.

Problems:
* `kFailedDnsResolutions`: One or more DNS resolutions resulted in a failure.
* `kFailedHttpsRequests`: One or more HTTPS requests resulted in a failure.
* `kHighLatency`: HTTPS request latency is high.
* `kVeryHighLatency`: HTTPS request latency is very high.

#### VideoConferencing

Tests the device's video conferencing capabilities by testing whether the device
can:
1. Contact either a default or specified STUN server via UDP.
2. Contact either a default or specified STUN server via TCP.
3. Reach common media endpoints.

Problems:
* `kPotentialProblemUdpFailure`: Failed requests to a STUN server via UDP.
* `kPotentialProblemTcpFailure`: Failed requests to a STUN server via TCP.
* `kPotentialProblemMediaFailure`: Failed to establish a TLS connection to media hostnames.
* `kPotentialProblemUdpAndMediaFailure`: Failed requests to a STUN server via
UDP and failed to establish a TLS connection to media hostnames.
* `kUdpAndTcpFailure`: Failed requests to a STUN server via UDP and TCP.
* `kTcpAndMediaFailure`: Failed requests to a STUN server via TCP and failed to
established a TLS connection to media hostnames.
* `kUdpAndTcpAndMediaFailure`: Failed requests to a STUN server via UDP and TCP,
and failed to establish a TLS connection to media hostnames.

[Network Health and Configuration]: https://docs.google.com/document/d/10DSy-jZXaRo9I9aq1UqERy76t7HkgGvInWk57pHEkzg
[network_diagnostics.mojom]: https://source.chromium.org/chromium/chromium/src/+/main:chromeos/services/network_health/public/mojom/network_diagnostics.mojom
[NetworkHealthService]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/net/network_health/network_health_service.h
[here]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/net/network_diagnostics/
[RoutineVerdict]: https://source.chromium.org/chromium/chromium/src/+/main:chromeos/services/network_health/public/mojom/network_diagnostics.mojom;l=12;drc=93304dcbcf58b0af39403af08928ea4e4ec28e6d
[Design Doc]: https://docs.google.com/document/d/1d5EoPBlsomWQ4HzqejFPG4v1d2cvPSndj7nmCjNZSSc
[src/platform2/diagnostics/mojo/network_diagnostics.mojom]: http://cs/chromeos_public/src/platform2/diagnostics/cros_healthd/network_diagnostics/
