// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_PERMISSION_REQUEST_CREATOR_APIARY_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_PERMISSION_REQUEST_CREATOR_APIARY_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/supervised_user/core/browser/permission_request_creator.h"
#include "google_apis/gaia/google_service_auth_error.h"

class GURL;
class Profile;

namespace signin {
class IdentityManager;
struct AccessTokenInfo;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}

// TODO(4598236): This class will be deprecated once we fully switch
// to make requests via the supervised_user::PermissionRequestCreatorImpl.
class PermissionRequestCreatorApiary
    : public supervised_user::PermissionRequestCreator {
 public:
  PermissionRequestCreatorApiary(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  PermissionRequestCreatorApiary(const PermissionRequestCreatorApiary&) =
      delete;
  PermissionRequestCreatorApiary& operator=(
      const PermissionRequestCreatorApiary&) = delete;

  ~PermissionRequestCreatorApiary() override;

  static std::unique_ptr<PermissionRequestCreator> CreateWithProfile(
      Profile* profile);

  // PermissionRequestCreator implementation:
  bool IsEnabled() const override;
  void CreateURLAccessRequest(const GURL& url_requested,
                              SuccessCallback callback) override;

 private:
  friend class PermissionRequestCreatorApiaryTest;

  struct Request;
  using RequestList = std::list<std::unique_ptr<Request>>;

  void OnAccessTokenFetchComplete(Request* request,
                                  GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);

  void OnSimpleLoaderComplete(RequestList::iterator it,
                              std::unique_ptr<std::string> response_body);

  GURL GetApiUrl() const;
  std::string GetApiScope() const;

  void CreateRequest(const std::string& request_type,
                     const std::string& object_ref,
                     SuccessCallback callback);

  // Requests an access token, which is the first thing we need. This is where
  // we restart when the returned access token has expired.
  void StartFetching(Request* request);

  void DispatchResult(RequestList::iterator it, bool success);

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  bool retry_on_network_change_;

  RequestList requests_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_PERMISSION_REQUEST_CREATOR_APIARY_H_
