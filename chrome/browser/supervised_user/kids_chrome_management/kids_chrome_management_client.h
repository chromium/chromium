// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_CHROME_MANAGEMENT_CLIENT_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_CHROME_MANAGEMENT_CLIENT_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class GoogleServiceAuthError;
class Profile;

// Provides an interface between Family Link RPC clients (e.g.
// KidsManagementURLChecker) and the Kids Chrome Management Service.
// Communicates with the server and performs HTTP requests.
class KidsChromeManagementClient : public KeyedService {
 public:
  enum class ErrorCode {
    kSuccess = 0,
    kTokenError,    // Failed to get OAuth2 token.
    kNetworkError,  // Network failure.
    kHttpError,     // HTTP error.
    kServiceError,  // Service returned an error or malformed reply.
  };

  using KidsChromeManagementCallback = base::OnceCallback<void(
      std::unique_ptr<google::protobuf::MessageLite> response_proto,
      ErrorCode error_code)>;

  explicit KidsChromeManagementClient(Profile* profile);

  ~KidsChromeManagementClient() override;

  // Each of the next three methods is the interface to an RPC client.
  // They receive only what's necessary for a request:
  //   - The request proto, specific to each RPC.
  //   - The callback that will receive the response proto and error code.

  // Interface to KidsManagementURLCheckerClient. Classifies a URL as safe
  // or restricted for a supervised user.
  virtual void ClassifyURL(
      std::unique_ptr<kids_chrome_management::ClassifyUrlRequest> request_proto,
      KidsChromeManagementCallback callback);

  // TODO(crbug.com/979618): implement ListFamilyMembers method.

  // TODO(crbug.com/979619): implement RequestRestrictedURLAccess method.

 private:
  // Every request must be represented by an instance of this struct. It will be
  // added to a request list and its iterator will be passed along to the
  // callbacks with request-specific information.
  struct KidsChromeManagementRequest;

  // Using a list ensures that iterators won't be invalidated when other
  // elements are added/erased.
  using KidsChromeRequestList =
      std::list<std::unique_ptr<KidsChromeManagementRequest>>;

  // Starts the execution flow by adding the request struct iterator to the
  // execution list.
  void MakeHTTPRequest(
      std::unique_ptr<KidsChromeManagementRequest> kids_chrome_request);

  // Fetches the user's access token.
  void StartFetching(KidsChromeRequestList::iterator it);

  void OnAccessTokenFetchComplete(
      KidsChromeRequestList::iterator kids_chrome_request,
      GoogleServiceAuthError auth_error,
      signin::AccessTokenInfo token_info);

  void OnSimpleLoaderComplete(
      KidsChromeRequestList::iterator kids_chrome_request,
      std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
      signin::AccessTokenInfo token_info,
      std::unique_ptr<std::string> response_body);

  // Calls the callback provided by the existing RPC client with the response
  // proto and/or error codes.
  void DispatchResult(
      KidsChromeRequestList::iterator kids_chrome_request,
      std::unique_ptr<google::protobuf::MessageLite> response_proto,
      ErrorCode error);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  signin::IdentityManager* identity_manager_;

  // List of requests in execution.
  KidsChromeRequestList requests_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(KidsChromeManagementClient);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_CHROME_MANAGEMENT_CLIENT_H_
