// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_IMPL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_IMPL_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace reporting {

class FileUploadDelegate : public FileUploadJob::Delegate {
 public:
  static constexpr int64_t kMaxUploadBufferSize = 1L * 1024L * 1024L;  // 1 MiB

  FileUploadDelegate();
  ~FileUploadDelegate() override;

  static std::string GetFileUploadUrl();

 private:
  // Helper classes.
  class AccessTokenRetriever;
  class InitContext;
  class NextStepContext;
  class FinalContext;

  friend class FileUploadDelegateTest;

  // FileUploadJob::Delegate:
  void DoInitiate(
      std::string_view origin_path,
      std::string_view upload_parameters,
      base::OnceCallback<void(
          StatusOr<std::pair<int64_t /*total*/,
                             std::string /*session_token*/>>)> cb) override;
  void DoNextStep(
      int64_t total,
      int64_t uploaded,
      std::string_view session_token,
      ScopedReservation scoped_reservation,
      base::OnceCallback<void(
          StatusOr<std::pair<int64_t /*uploaded*/,
                             std::string /*session_token*/>>)> cb) override;
  void DoFinalize(
      std::string_view session_token,
      base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)> cb)
      override;

  void DoDeleteFile(std::string_view origin_path) override;

  // Called once authentication is finished (with token or failure status).
  void OnAccessTokenResult(
      std::string_view origin_path,
      std::string_view upload_parameters,
      base::OnceCallback<void(
          StatusOr<
              std::pair<int64_t /*total*/, std::string /*session_token*/>>)> cb,
      StatusOr<std::string> access_token_result);

  // Initializes the delegate once, lazily - when the first API is called.
  // On later API calls this method immediately returns.
  void InitializeOnce();

  // Helper method starts OAuth2 token request.
  [[nodiscard]] std::unique_ptr<OAuth2AccessTokenManager::Request>
  StartOAuth2Request(
      OAuth2AccessTokenManager::Consumer* consumer  // owned by the caller!
  ) const;

  // Helper method populates rest of request and creates SimpleURLLoader
  // instance.
  [[nodiscard]] std::unique_ptr<::network::SimpleURLLoader> CreatePostLoader(
      std::unique_ptr<::network::ResourceRequest> resource_request) const;

  // Helper method sends request and hands the response headers to `response_cb`
  // (on the current task runner).
  void SendAndGetResponse(
      ::network::SimpleURLLoader* url_loader,  // owned by the caller!
      base::OnceCallback<void(scoped_refptr<::net::HttpResponseHeaders>
                                  headers)> response_cb) const;

  base::WeakPtr<FileUploadDelegate> GetWeakPtr();

  SEQUENCE_CHECKER(sequence_checker_);

  // The URL to which the POST request should be directed.
  GURL upload_url_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The account ID that will be used for the access token fetch.
  CoreAccountId account_id_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The token manager used to retrieve the access token (not owned).
  raw_ptr<OAuth2AccessTokenManager> access_token_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // This is used to initialize the network::SimpleURLLoader object.
  scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Network traffic annotation set by the delegate describing what kind of data
  // is uploaded.
  std::unique_ptr<net::NetworkTrafficAnnotationTag> traffic_annotation_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Maximum upload size allowed for a single request.
  int64_t max_upload_buffer_size_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Weak pointer factory used by this delegate.
  // Note that weak pointers here are all dereferenced on UI task runner, and so
  // the factory needs to be reset there as well - because of that we make it
  // moveable by using smart pointer.
  std::unique_ptr<base::WeakPtrFactory<FileUploadDelegate>> weak_ptr_factory_{
      new base::WeakPtrFactory<FileUploadDelegate>{this}};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_IMPL_H_
