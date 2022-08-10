// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_EXTERNAL_FETCHER_H_
#define CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_EXTERNAL_FETCHER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chrome::kids {

// The receiver of fetch must implement FetcherDelegate, or provide an instance
// that will outlive the request.
template <typename Response>
class FetcherDelegate {
 public:
  virtual void OnSuccess(std::unique_ptr<Response> response) = 0;
  virtual void OnFailure(base::StringPiece response_body) = 0;
  virtual void OnMalformedResponse(base::StringPiece response_body) = 0;
};

// Use instance of Fetcher to start request and write the result onto the
// receiving delegate. Every instance of Fetcher is disposable and should be
// used only once.
template <typename Request, typename Response>
class Fetcher {
 public:
  virtual void StartRequest(const Request& request,
                            base::StringPiece access_token,
                            base::StringPiece url) = 0;
  virtual ~Fetcher() = default;
};

// Creates a disposable instance of a Fetcher for ListFamilyMembers.
std::unique_ptr<Fetcher<kids_chrome_management::ListFamilyMembersRequest,
                        kids_chrome_management::ListFamilyMembersResponse>>
CreateListFamilyMembersFetcher(
    FetcherDelegate<kids_chrome_management::ListFamilyMembersResponse>&
        delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

}  // namespace chrome::kids

#endif  // CHROME_BROWSER_SUPERVISED_USER_KIDS_CHROME_MANAGEMENT_KIDS_EXTERNAL_FETCHER_H_
