// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fake_device_metadata_http_fetcher.h"

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "device/bluetooth/bluetooth_device.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"

namespace ash {
namespace quick_pair {

namespace {

constexpr char kValidResponseEncoded[] =
    "Cr0HCKy4kAMYASJ4aHR0cHM6Ly9saDMuZ29vZ2xldXNlcmNvbnRlbnQuY29tL3dLM3YzcVI1d3"
    "pHMk5JbXBuS1UyYlpfblFpdjh4elJoVDFadWRhT0NhSzlOVzRVS3lZNWtvYlNrSHlxeUJZTzVO"
    "M1h3Um84XzRERkdGcHEtUjNWbW5nKgpQaXhlbCBCdWRzMrQCaW50ZW50OiNJbnRlbnQ7YWN0aW"
    "9uPWNvbS5nb29nbGUuYW5kcm9pZC5nbXMubmVhcmJ5LmRpc2NvdmVyeSUzQUFDVElPTl9NQUdJ"
    "Q19QQUlSO3BhY2thZ2U9Y29tLmdvb2dsZS5hbmRyb2lkLmdtcztjb21wb25lbnQ9Y29tLmdvb2"
    "dsZS5hbmRyb2lkLmdtcy8ubmVhcmJ5LmRpc2NvdmVyeS5zZXJ2aWNlLkRpc2NvdmVyeVNlcnZp"
    "Y2U7Uy5jb20uZ29vZ2xlLmFuZHJvaWQuZ21zLm5lYXJieS5kaXNjb3ZlcnklM0FFWFRSQV9DT0"
    "1QQU5JT05fQVBQPWNvbS5nb29nbGUuYW5kcm9pZC5hcHBzLndlYXJhYmxlcy5tYWVzdHJvLmNv"
    "bXBhbmlvbjtlbmRFmpkZP0pCEkAqaamccP9akybjAVzzvMeRhVAJjHOrKnyKet1/"
    "L9H3JlQQdpFD2w1lclPx5B9I2+mjvU9IHsW2Xlsn6z0+HgdTUgIIA1oMCNye8e4FEIDe/"
    "OIBaAd67QIKeGh0dHBzOi8vbGgzLmdvb2dsZXVzZXJjb250ZW50LmNvbS9POUo4V0dSQzg5Q08"
    "1OEFTamFGbWxjbVM1NWYtMWJJZjJvUWFXVzFyS2REMDZtRlpTS1E3RWlNT3JKSWpjRUtRR0RTL"
    "UlwUmN2TndhUGhvWXA1YVFuZxJ2aHR0cHM6Ly9saDMuZ29vZ2xldXNlcmNvbnRlbnQuY29tL1l"
    "YeWk4Vy1VQkRKYklySU40cnhUQXljR2dSb2lXTWNuUFpsVnJRMU1MTHl1WjJEbVBIdGJzRDIzY"
    "m1leTdJbXlHZWpSbWR2YWZIS0tzbXV6Zm5mVBp5aHR0cHM6Ly9saDMuZ29vZ2xldXNlcmNvbnR"
    "lbnQuY29tL2hXeWdJM2liTjRIV0QtRmp3dlNuV1BIbkdzMUFNMTVLclh2VWx6Tmo2SlJDMmpNM"
    "jZuTFZjY0tYNDFRZEtmOHE3aFN2eVJMYjZMcVJCRDJWVTlQWkNQY4ABAZoBBkdvb2dsZaoBF1B"
    "yZXN0byBFVlQgQWxtb3N0IEJsYWNrsAECugEAGpEBiVBORw0KGgoAAAANSUhEUgAAAAQAAAAEC"
    "AYAAACp8Z5+"
    "AAAABHNCSVQICAgIfAhkiAAAAEhJREFUCJkFwTENgDAQQNGfYIAEXdjARTVUARI6UBMsnU9ADd"
    "zM0Hze2wBKKfda65lzvqhna83eu+qOemWmESEA6jHG+NQK8AOtZCpIT/"
    "9elAAAAABJRU5ErkJggiKjBRInVGFwIHRvIHBhaXIuIEVhcmJ1ZHMgd2lsbCBiZSB0aWVkIHRv"
    "ICVzGhxUYXAgdG8gcGFpciB3aXRoIHRoaXMgZGV2aWNlIhNUYXAgdG8gZmluaXNoIHNldHVwKi"
    "5UYXAgdG8gdXBkYXRlIGRldmljZSBzZXR0aW5ncyBhbmQgZmluaXNoIHNldHVwMj5UYXAgdG8g"
    "ZG93bmxvYWQgZGV2aWNlIGFwcCBvbiBHb29nbGUgUGxheSBhbmQgc2VlIGFsbCBmZWF0dXJlcz"
    "oRVW5hYmxlIHRvIGNvbm5lY3RCIlRyeSBtYW51YWxseSBwYWlyaW5nIHRvIHRoZSBkZXZpY2VK"
    "KCVzIHdpbGwgYXBwZWFyIG9uIGRldmljZXMgbGlua2VkIHdpdGggJXNSIVlvdXIgZGV2aWNlIG"
    "lzIHJlYWR5IHRvIGJlIHNldCB1cFpERG93bmxvYWQgdGhlIGRldmljZSBhcHAgb24gR29vZ2xl"
    "IFBsYXkgdG8gc2VlIGFsbCBhdmFpbGFibGUgZmVhdHVyZXNiGENvbm5lY3QgJXMgdG8gdGhpcy"
    "BwaG9uZWo6U2F2ZSBkZXZpY2UgdG8gJXMgZm9yIGZhc3RlciBwYWlyaW5nIHRvIHlvdXIgb3Ro"
    "ZXIgZGV2aWNlc3IcVGhpcyB3aWxsIHRha2UgYSBmZXcgbW9tZW50c3o3VHJ5IG1hbnVhbGx5IH"
    "BhaXJpbmcgdG8gdGhlIGRldmljZSBieSBnb2luZyB0byBTZXR0aW5nc7IBN0dldCB0aGUgaGFu"
    "ZHMtZnJlZSBoZWxwIG9uIHRoZSBnbyBmcm9tIEdvb2dsZSBBc3Npc3RhbnS6ASNUYXAgdG8gc2"
    "V0IHVwIHlvdXIgR29vZ2xlIEFzc2lzdGFudA==";
constexpr char kInvalidResponse[] = "<html>404 error</html>";
constexpr char kValidUrl[] =
    "https://nearbydevices-pa.googleapis.com/v1/device/2748";

}  // namespace

FakeDeviceMetadataHttpFetcher::FakeDeviceMetadataHttpFetcher()
    : HttpFetcher() {}

FakeDeviceMetadataHttpFetcher::~FakeDeviceMetadataHttpFetcher() = default;

void FakeDeviceMetadataHttpFetcher::ExecuteGetRequest(
    const GURL& url,
    FetchCompleteCallback callback) {
  num_gets_++;

  if (has_network_error_) {
    std::move(callback).Run(std::make_unique<std::string>(kInvalidResponse),
                            nullptr);
    return;
  }

  if (base::StartsWith(url.spec(), kValidUrl)) {
    LOG(ERROR) << "executing valid url cb";
    std::string decoded;
    base::Base64Decode(kValidResponseEncoded, &decoded);
    std::move(callback).Run(
        std::make_unique<std::string>(decoded),
        std::make_unique<FastPairHttpResult>(net::Error::OK, nullptr));
    return;
  }
  LOG(ERROR) << "executing invalid url cb " << url.spec()
             << " != " << kValidUrl;
  std::move(callback).Run(
      nullptr,
      std::make_unique<FastPairHttpResult>(
          net::OK,
          network::CreateURLResponseHead(net::HttpStatusCode::HTTP_OK).get()));
}

}  // namespace quick_pair
}  // namespace ash
