// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_EXTERNAL_FETCHER_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_EXTERNAL_FETCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chrome::kids {
// Use instance of Fetcher to start request and write the result onto the
// receiving delegate. Every instance of Fetcher is disposable and should be
// used only once.
template <typename Request, typename Response>
class Fetcher {
 public:
  enum Error {
    NONE,
    INPUT_ERROR,  // The request could not be performed due to prerequisites
                  // error.
    HTTP_ERROR,   // The request was performed, but http returned errors.
    PARSE_ERROR,  // The request was performed without error, but http response
                  // could not be parsed.
  };
  using Callback = base::OnceCallback<void(Error, Response)>;

  virtual void StartRequest(base::StringPiece url,
                            const Request& request,
                            base::expected<signin::AccessTokenInfo,
                                           GoogleServiceAuthError> access_token,
                            Callback callback) = 0;

  virtual ~Fetcher() = default;
};

// Creates a disposable instance of a Fetcher for ListFamilyMembers.
std::unique_ptr<Fetcher<kids_chrome_management::ListFamilyMembersRequest,
                        kids_chrome_management::ListFamilyMembersResponse>>
CreateListFamilyMembersFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

}  // namespace chrome::kids

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_EXTERNAL_FETCHER_H_
