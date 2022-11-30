// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace enterprise_connectors {

class WinNetworkFetcher;
class WinNetworkFetcherFactory;

// Windows implementation of the KeyNetworkDelegate interface.
class WinKeyNetworkDelegate : public KeyNetworkDelegate {
 public:
  WinKeyNetworkDelegate();
  ~WinKeyNetworkDelegate() override;

  // KeyNetworkDelegate:
  void SendPublicKeyToDmServer(
      const GURL& url,
      const std::string& dm_token,
      const std::string& body,
      UploadKeyCompletedCallback upload_key_completed_callback) override;

 private:
  friend class WinKeyNetworkDelegateTest;

  explicit WinKeyNetworkDelegate(
      std::unique_ptr<WinNetworkFetcherFactory> factory);

  // Makes an upload key request to the windows network fetcher. The
  // `upload_key_completed_callback` will be invoked after the upload request,
  // in the FetchCompleted method.
  void UploadKey(UploadKeyCompletedCallback upload_key_completed_callback);

  // Invokes `upload_key_completed_callback` with the HTTP `response_code`.
  void FetchCompleted(UploadKeyCompletedCallback upload_key_completed_callback,
                      HttpResponseCode response_code);

  // Used for creating the WinNetworkFetcher object.
  std::unique_ptr<WinNetworkFetcherFactory> win_network_fetcher_factory_;

  // Used for issuing network requests via the winhttp network fetcher.
  std::unique_ptr<WinNetworkFetcher> win_network_fetcher_;

  // Used for exponential back off for retryable network errors.
  net::BackoffEntry backoff_entry_;

  base::WeakPtrFactory<WinKeyNetworkDelegate> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_
