// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_FETCHER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/dial_url_fetcher.h"
#include "url/gurl.h"

namespace media_router {

struct DialDeviceDescriptionData;

// Used to make a single HTTP GET request with |device_description_url| to fetch
// a uPnP device description from a DIAL device.  If successful, |success_cb| is
// invoked with the result; otherwise, |error_cb| is invoked with an error
// reason.
// This class is not sequence safe.
class DeviceDescriptionFetcher {
 public:
  DeviceDescriptionFetcher(
      const DialDeviceData& device_data,
      base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
      base::OnceCallback<void(const std::string&)> error_cb);

  DeviceDescriptionFetcher(const DeviceDescriptionFetcher&) = delete;
  DeviceDescriptionFetcher& operator=(const DeviceDescriptionFetcher&) = delete;

  virtual ~DeviceDescriptionFetcher();

  const GURL& device_description_url() const {
    return device_data_.device_description_url();
  }

  // Marked virtual for tests.
  virtual void Start();

 private:
  friend class TestDeviceDescriptionFetcher;

  // Processes the response from the GET request and invoke the success or
  // error callback.
  void ProcessResponse(const std::string& response);

  // Runs |error_cb_| with |message| and clears it.
  void ReportError(const std::string& message,
                   std::optional<int> response_code = std::nullopt);

  const DialDeviceData device_data_;

  base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb_;
  base::OnceCallback<void(const std::string&)> error_cb_;
  std::unique_ptr<DialURLFetcher> fetcher_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_FETCHER_H_
