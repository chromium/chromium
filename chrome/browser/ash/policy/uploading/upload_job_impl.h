// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_UPLOADING_UPLOAD_JOB_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_UPLOADING_UPLOAD_JOB_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/ash/policy/uploading/upload_job.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class HttpResponseHeaders;
}

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace policy {

// This implementation of UploadJob uses the OAuth2AccessTokenManager to acquire
// access tokens for the device management (cloud-based policy) server scope and
// uses a SimpleURLLoader to upload data to the specified upload url.
class UploadJobImpl : public UploadJob,
                      public OAuth2AccessTokenManager::Consumer {
 public:
  // UploadJobImpl uses a MimeBoundaryGenerator to generate strings which
  // mark the boundaries between data segments.
  class MimeBoundaryGenerator {
   public:
    MimeBoundaryGenerator& operator=(const MimeBoundaryGenerator&) = delete;

    virtual ~MimeBoundaryGenerator();

    virtual std::string GenerateBoundary() const = 0;
  };

  // An implemenation of the MimeBoundaryGenerator which uses random
  // alpha-numeric characters to construct MIME boundaries.
  class RandomMimeBoundaryGenerator : public MimeBoundaryGenerator {
   public:
    ~RandomMimeBoundaryGenerator() override;

    std::string GenerateBoundary() const override;  // MimeBoundaryGenerator
  };

  // |task_runner| must belong to the same thread from which the constructor and
  // all the public methods are called.
  UploadJobImpl(
      const GURL& upload_url,
      const CoreAccountId& account_id,
      OAuth2AccessTokenManager* access_token_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Delegate* delegate,
      std::unique_ptr<MimeBoundaryGenerator> boundary_generator,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  UploadJobImpl(const UploadJobImpl&) = delete;
  UploadJobImpl& operator=(const UploadJobImpl&) = delete;

  ~UploadJobImpl() override;

  // UploadJob:
  void AddDataSegment(const std::string& name,
                      const std::string& filename,
                      const std::map<std::string, std::string>& header_entries,
                      std::unique_ptr<std::string> data) override;
  void Start() override;

  // Sets the retry delay to a shorter time to prevent browser tests from
  // timing out.
  static void SetRetryDelayForTesting(long retryDelayMs);

 private:
  // Indicates the current state of the UploadJobImpl.
  // State transitions for successful upload:
  //   IDLE -> ACQUIRING_TOKEN -> PREPARING_CONTENT -> UPLOADING -> SUCCESS
  // If error happens, state goes back to ACQUIRING_TOKEN.
  // State transitions when error occurs once:
  //   IDLE -> ACQUIRING_TOKEN -> PREPARING_CONTENT -> UPLOADING ->
  //     -> ACQUIRING_TOKEN -> PREPARING_CONTENT -> UPLOADING -> SUCCESS
  // State transitions when tried unsuccessfully kMaxRetries times:
  //   ... -> PREPARING_CONTENT -> UPLOADING -> ERROR
  enum State {
    IDLE,               // Start() has not been called.
    ACQUIRING_TOKEN,    // Trying to acquire the access token.
    PREPARING_CONTENT,  // Currently encoding the content.
    UPLOADING,          // Upload started.
    SUCCESS,            // Upload successfully completed.
    ERROR               // Upload failed.
  };

  // OAuth2AccessTokenManager::Consumer:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Called when the SimpleURLLoader is finished.
  void OnURLLoadComplete(scoped_refptr<net::HttpResponseHeaders> headers);

  void HandleError(ErrorCode errorCode);

  // Requests an access token for the upload scope.
  void RequestAccessToken();

  // Dispatches POST request.
  void StartUpload();

  // Constructs the body of the POST request by concatenating the
  // |data_segments_|, separated by appropriate content-disposition headers and
  // a MIME boundary. Places the request body in |post_data_| and a copy of the
  // MIME boundary in |mime_boundary_|. Returns true on success. If |post_data_|
  // and |mime_boundary_| were set already, returns true immediately. In case of
  // an error, clears |post_data_| and |mime_boundary_| and returns false.
  bool SetUpMultipart();

  // Assembles the request and starts the SimpleURLLoader. Fails if another
  // upload is still in progress or the content was not successfully encoded.
  void CreateAndStartURLLoader(const std::string& access_token);

  // The URL to which the POST request should be directed.
  const GURL upload_url_;

  // The account ID that will be used for the access token fetch.
  const CoreAccountId account_id_;

  // The token manager used to retrieve the access token.
  const raw_ptr<OAuth2AccessTokenManager> access_token_manager_;

  // This is used to initialize the network::SimpleURLLoader object.
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The delegate to be notified of events.
  const raw_ptr<Delegate> delegate_;

  // An implementation of MimeBoundaryGenerator. This instance will be used to
  // generate MIME boundaries when assembling the multipart request in
  // SetUpMultipart().
  std::unique_ptr<MimeBoundaryGenerator> boundary_generator_;

  // Network traffic annotation set by the delegate describing what kind of data
  // is uploaded.
  net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Current state the uploader is in.
  State state_;

  // Contains the cached MIME boundary.
  std::unique_ptr<std::string> mime_boundary_;

  // Contains the cached, encoded post data.
  std::unique_ptr<std::string> post_data_;

  // Keeps track of the number of retries.
  int retry_;

  // The OAuth request to receive the access token.
  std::unique_ptr<OAuth2AccessTokenManager::Request> access_token_request_;

  // The OAuth access token.
  std::string access_token_;

  // Helper to upload the data.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // The data chunks to be uploaded.
  std::vector<std::unique_ptr<DataSegment>> data_segments_;

  // TaskRunner used for scheduling retry attempts.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::ThreadChecker thread_checker_;

  // Should remain the last member so it will be destroyed first and
  // invalidate all weak pointers.
  base::WeakPtrFactory<UploadJobImpl> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_UPLOADING_UPLOAD_JOB_IMPL_H_
