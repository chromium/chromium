// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_UPLOADER_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_UPLOADER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

class SafeBrowsingTokenFetcher;

// An uploader of extension telemetry reports. An upload is initiated by
// creating an instance of this object and then calling its StartUpload method.
// The upload can be cancelled by deleting the uploader instance. The instance
// is not usable after the upload notification and can be safely destroyed.
class ExtensionTelemetryUploader {
 public:
  ExtensionTelemetryUploader(const ExtensionTelemetryUploader&) = delete;
  ExtensionTelemetryUploader& operator=(const ExtensionTelemetryUploader&) =
      delete;

  virtual ~ExtensionTelemetryUploader();

  // A callback run by the uploader upon success or failure.
  using OnUploadCallback =
      base::OnceCallback<void(bool success, const std::string& response_data)>;

  ExtensionTelemetryUploader(
      OnUploadCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      std::unique_ptr<std::string> upload_data,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher);

  // Start the upload by sending a request. This method performs retries if
  // necessary and finally calls |callback_|. It must be called on the UI
  // thread. |callback_| will also be invoked on the UI thread.
  void Start();

  static std::string GetUploadURLForTest();

 private:
  // Determines whether to send a request with access token.
  void MaybeSendRequestWithAccessToken();

  // Sends a single network request.
  void SendRequest(const std::string& access_token);

  // Callback when SimpleURLLoader gets the response.
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // Called by OnURLLoaderComplete to handle successful/failed upload.
  void RetryOrFinish(int net_error,
                     int response_code,
                     const std::string& response_data);

  OnUploadCallback callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  // Serialized telemetry report.
  std::unique_ptr<std::string> upload_data_;
  // Tracks backoff and retry parameters.
  base::TimeDelta current_backoff_;
  int num_upload_retries_;
  base::TimeTicks upload_start_time_;
  // The token fetcher used to attach OAuth access tokens to requests for
  // appropriately consented users. It can be a nullptr when the user is
  // not signed in.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  base::WeakPtrFactory<ExtensionTelemetryUploader> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_UPLOADER_H_
