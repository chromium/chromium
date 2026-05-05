// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_AUTHENTICATED_NETWORK_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_AUTHENTICATED_NETWORK_FETCHER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/update_client/network.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// A NetworkFetcher wrapper that implements "authentication hunting" for Google
// domains during file downloads. If `DownloadToFile` fails with an auth error
// (HTTP 401 or 403), this fetcher will internally retry the request by
// incrementing the "authuser" query parameter. Authentication hunting is not
// implemented for `PostRequest`.
class AuthenticatedNetworkFetcher : public update_client::NetworkFetcher {
 public:
  explicit AuthenticatedNetworkFetcher(
      scoped_refptr<update_client::NetworkFetcherFactory> base_factory);

  AuthenticatedNetworkFetcher(const AuthenticatedNetworkFetcher&) = delete;
  AuthenticatedNetworkFetcher& operator=(const AuthenticatedNetworkFetcher&) =
      delete;

  ~AuthenticatedNetworkFetcher() override;

  // update_client::NetworkFetcher:
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override;

 private:
  class DownloadToFileState;
  void OnDownloadResponseStarted(const GURL& url,
                                 ResponseStartedCallback callback,
                                 scoped_refptr<DownloadToFileState> state,
                                 int response_code,
                                 int64_t content_length);

  void OnDownloadToFileComplete(
      GURL url,
      base::FilePath file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      scoped_refptr<DownloadToFileState> state,
      int net_error,
      int64_t content_size);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<update_client::NetworkFetcherFactory> base_factory_;

  base::WeakPtrFactory<AuthenticatedNetworkFetcher> weak_ptr_factory_{this};
};

class AuthenticatedNetworkFetcherFactory
    : public update_client::NetworkFetcherFactory {
 public:
  explicit AuthenticatedNetworkFetcherFactory(
      scoped_refptr<update_client::NetworkFetcherFactory> base_factory);

  AuthenticatedNetworkFetcherFactory(
      const AuthenticatedNetworkFetcherFactory&) = delete;
  AuthenticatedNetworkFetcherFactory& operator=(
      const AuthenticatedNetworkFetcherFactory&) = delete;

  std::unique_ptr<update_client::NetworkFetcher> Create() const override;

 protected:
  ~AuthenticatedNetworkFetcherFactory() override;

 private:
  scoped_refptr<update_client::NetworkFetcherFactory> base_factory_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_AUTHENTICATED_NETWORK_FETCHER_H_
