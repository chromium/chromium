// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_EXTERNAL_FETCHER_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_EXTERNAL_FETCHER_H_

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_access_token_fetcher.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Use instance of Fetcher to start request and write the result onto the
// receiving delegate. Every instance of Fetcher is disposable and should be
// used only once.
template <typename Request, typename Response>
class KidsExternalFetcher {
 public:
  enum Error {
    NONE,
    AUTHENTICATION_ERROR,  // The request could not be performed because the
                           // peer's identity could not be verified.
    HTTP_ERROR,        // The request was performed, but http returned errors.
    INVALID_RESPONSE,  // The request was performed without error, but http
                       // response could not be processed or was unexpected.
  };
  using Callback = base::OnceCallback<void(Error, std::unique_ptr<Response>)>;
  virtual ~KidsExternalFetcher() = default;
};

// Creates a disposable instance of an access token consumer that will fetch
// list of family members.
std::unique_ptr<
    KidsExternalFetcher<kids_chrome_management::ListFamilyMembersRequest,
                        kids_chrome_management::ListFamilyMembersResponse>>
FetchListFamilyMembers(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::StringPiece url,
    KidsExternalFetcher<
        kids_chrome_management::ListFamilyMembersRequest,
        kids_chrome_management::ListFamilyMembersResponse>::Callback callback);

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_EXTERNAL_FETCHER_H_
