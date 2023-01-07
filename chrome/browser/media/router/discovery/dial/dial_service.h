// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_SERVICE_H_

namespace media_router {

class DialDeviceData;

// DialService accepts requests to discover devices, sends multiple SSDP
// M-SEARCH requests via UDP multicast, and notifies observers when a
// DIAL-compliant device responds.
//
// The syntax of the M-SEARCH request and response is defined by Section 1.3
// of the uPnP device architecture specification and related documents:
//
// http://upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.1.pdf
//
// Each time Discover() is called, kDialNumRequests M-SEARCH requests are sent
// (with a delay of kDialRequestIntervalMillis in between):
//
// Time    Action
// ----    ------
// T1      Request 1 sent, OnDiscoveryReqest() called
// ...
// Tk      Request kDialNumRequests sent, OnDiscoveryReqest() called
// Tf      OnDiscoveryFinished() called
//
// Any time a valid response is received between T1 and Tf, it is parsed and
// OnDeviceDiscovered() is called with the result.  Tf is set to Tk +
// kDialResponseTimeoutSecs (the response timeout passed in each request).
//
// Calling Discover() again between T1 and Tf has no effect.
//
// All relevant constants are defined in dial_service.cc.
class DialService {
 public:
  enum DialServiceErrorCode {
    DIAL_SERVICE_NO_INTERFACES = 0,
    DIAL_SERVICE_SOCKET_ERROR
  };

  class Client {
   public:
    // Called when a single discovery request was sent.
    virtual void OnDiscoveryRequest() = 0;

    // Called when a device responds to a request.
    virtual void OnDeviceDiscovered(const DialDeviceData& device) = 0;

    // Called when we have all responses from the last discovery request.
    virtual void OnDiscoveryFinished() = 0;

    // Called when an error occurs.
    virtual void OnError(DialServiceErrorCode code) = 0;

   protected:
    virtual ~Client() = default;
  };

  virtual ~DialService() {}

  // Starts a new round of discovery.  Returns |true| if discovery was started
  // successfully or there is already one active. Returns |false| on error.
  virtual bool Discover() = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_SERVICE_H_
