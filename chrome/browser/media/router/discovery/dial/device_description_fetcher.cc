// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/device_description_fetcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "net/base/ip_address.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

constexpr char kApplicationUrlHeaderName[] = "Application-URL";

namespace media_router {

DeviceDescriptionFetcher::DeviceDescriptionFetcher(
    const DialDeviceData& device_data,
    base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
    base::OnceCallback<void(const std::string&)> error_cb)
    : device_data_(device_data),
      success_cb_(std::move(success_cb)),
      error_cb_(std::move(error_cb)) {}

DeviceDescriptionFetcher::~DeviceDescriptionFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DeviceDescriptionFetcher::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!fetcher_);

  fetcher_ = std::make_unique<DialURLFetcher>(
      base::BindOnce(&DeviceDescriptionFetcher::ProcessResponse,
                     base::Unretained(this)),
      base::BindOnce(&DeviceDescriptionFetcher::ReportError,
                     base::Unretained(this)));

  fetcher_->Get(device_description_url(), false /** set_origin_header **/);
}

void DeviceDescriptionFetcher::ProcessResponse(const std::string& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(fetcher_);

  const network::mojom::URLResponseHead* response_info =
      fetcher_->GetResponseHead();
  DCHECK(response_info);

  // NOTE: The uPnP spec requires devices to set a Content-Type: header of
  // text/xml; charset="utf-8" (sec 2.11).  However Chromecast (and possibly
  // other devices) do not comply, so specifically not checking this header.
  std::string app_url_header;
  if (!response_info->headers ||
      !response_info->headers->GetNormalizedHeader(kApplicationUrlHeaderName,
                                                   &app_url_header) ||
      app_url_header.empty()) {
    ReportError("Missing or empty Application-URL:");
    return;
  }

  // Section 5.4 of the DIAL spec implies that the Application URL should not
  // have path, query or fragment...unsure if that can be enforced.
  GURL app_url(app_url_header);
  if (!device_data_.IsValidUrl(app_url)) {
    ReportError(base::StringPrintf("Invalid Application-URL: %s",
                                   app_url_header.c_str()));
    return;
  }

  // Remove trailing slash if there is any.
  if (app_url.ExtractFileName().empty()) {
    app_url = GURL(app_url_header.substr(0, app_url_header.length() - 1));
  }

  std::move(success_cb_).Run(DialDeviceDescriptionData(response, app_url));
}

void DeviceDescriptionFetcher::ReportError(const std::string& message,
                                           std::optional<int> response_code) {
  std::move(error_cb_).Run(message);
}

}  // namespace media_router
