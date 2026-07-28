// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_REQUEST_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_REQUEST_HELPER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class GoogleServiceAuthError;

namespace password_manager {

// Helper class that handles OAuth2 authentication and making network requests
// to Remote Actor backend services.
//
// This is a one-shot helper. It manages the token fetch and URL loader
// lifecycle. The caller must keep this object alive until `completion_callback`
// is invoked. The callback is always run asynchronously (even on immediate
// errors). It is safe to delete the `RemoteActorRequest` instance inside the
// callback (e.g., by posting a deletion task to the current task runner).
//
// Sequence of operations:
//
// Client            RemoteActorRequest     IdentityManager      Backend Service
//   |                       |                     |                    |
//   |--- (construct) ------>|                     |                    |
//   |                       |                     |                    |
//   |--- Start() ---------->|                     |                    |
//   |                       |--- Request Token -->|                    |
//   |                       |<-- Access Token ----|                    |
//   |                       |                                          |
//   |                       |--- HTTP Request (with token) ----------->|
//   |                       |<-- HTTP Response (200/204/5xx) ----------|
//   |<-- CompletionCb ------|                                          |
//   |    (req, success)     |                                          |
//
class RemoteActorRequest {
 public:
  using CompletionCallback =
      base::OnceCallback<void(RemoteActorRequest*, bool)>;

  RemoteActorRequest(
      signin::IdentityManager* identity_manager,
      const GURL& url,
      const std::string& method,
      const std::string& post_data,
      signin::OAuthConsumerId consumer_id,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      CompletionCallback completion_callback);
  RemoteActorRequest(const RemoteActorRequest&) = delete;
  RemoteActorRequest& operator=(const RemoteActorRequest&) = delete;
  ~RemoteActorRequest();

  // Starts the request by fetching the access token.
  void Start();

  // Returns true if the request completed successfully (HTTP 200/204 and Net
  // OK).
  bool GetSuccess() const;

 private:
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);
  void OnSimpleLoaderComplete(std::optional<std::string> response_body);

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  GURL url_;
  std::string method_;
  std::string post_data_;
  signin::OAuthConsumerId consumer_id_;
  net::NetworkTrafficAnnotationTag traffic_annotation_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  CompletionCallback completion_callback_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_REQUEST_HELPER_H_
