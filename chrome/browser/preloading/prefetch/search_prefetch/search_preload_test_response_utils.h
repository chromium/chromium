// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PRELOAD_TEST_RESPONSE_UTILS_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PRELOAD_TEST_RESPONSE_UTILS_H_

#include <queue>

#include "base/functional/callback_forward.h"
#include "base/thread_annotations.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Used by SearchPreloadDeferrableResponse and related testing code, to indicate
// whether and what to defer during testing.
enum class SearchPreloadTestResponseDeferralType {
  // Do not defer HTTP responses.
  kNoDeferral = 0,
  // Ddefer the response header only.
  kDeferHeader = 1,
  // Only defer the response body.
  kDeferBody = 2,
  // Defer dispatching response head until a explicit signal, and then block
  // the response until receiving the next signal.
  kDeferHeaderThenBody = 3,
  // Send headers immediately, but defer dispatching the first part of response
  // body and then the remaining part with the complete signal. Note: Callers
  // should guarantee it has provided a valid response content whose size is
  // greater than 1, so that server can split the body.
  kDeferChunkedResponseBody = 4
};

// A test base that allows test fixtures to control when and what to respond.
// For a test class that wants to defer response, it can derive this class, and
// implement its method to utilize the delayed response.
// Sample Usage:
// Define test fixtures:
// Class FooBrowserTest: public SearchPreloadResponseController {
//   ...
//     std::unique_ptr<net::test_server::HttpResponse>
//     HandleSearchRequest(const net::test_server::HttpRequest& request) {
//       // Construct a delayed response.
// ..... Figure out headers, response code, response body, etc.
//       return CreateDeferrableResponse(args);
//     }
//
// };
// For tests:
//  step 1: sets deferral type:
//  set_deferral_type();
//  step 2: make chrome send a request to the server, so then
//  HandleSearchRequest would be executed.
//  step 3: do something
//  step 4: dispatch the delayed part of response by calling
//  DispatchDelayedResponseTask.
// TODO(crbug.com/40219294): This class should be a part of
// SearchPrefetchBaseBrowserTest. Eliminate the differences between
// SearchPreloadUnifiedBrowserTest and SearchPrefetchBaseBrowserTest, such as
// removing duplicated methods from SearchPreloadUnifiedBrowserTest and making
// search prefetch tests run on Android (blocked by Android UI support), so as
// to get rid of this workaround.
class SearchPreloadResponseController {
 public:
  SearchPreloadResponseController();
  virtual ~SearchPreloadResponseController();

  // Called on the thread the server is running. The custom defined responses
  // should call this method if they want to defer the network response.
  void AddDelayedResponseTask(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::OnceClosure response_closure);

 protected:
  // Called on the main thread. This will resume one delayed response.
  void DispatchDelayedResponseTask();

  // Called on the thread that server is running on. The server could create a
  // SearchPreloadDeferrableResponse so that then can control when to dispatch
  // the task that would update the response.
  std::unique_ptr<net::test_server::HttpResponse> CreateDeferrableResponse(
      net::HttpStatusCode code,
      const base::StringPairs& headers,
      const std::string& response_body);

  // Instructs the search service whether to delay the response until
  // receiving a specific signal (From callers' prospective, calling
  // `DispatchDelayedResponseTask`). See comment of
  // `SearchPreloadTestResponseDeferralType` for more information.
  void set_service_deferral_type(
      SearchPreloadTestResponseDeferralType service_deferral_type) {
    service_deferral_type_ = service_deferral_type;
  }

 private:
  // A DelayedResponseTask instance is created on the thread that server is
  // running on, and be destroyed on the main thread. A lock is guarding the
  // access to created instances.
  class DelayedResponseTask;

  SearchPreloadTestResponseDeferralType service_deferral_type_ =
      SearchPreloadTestResponseDeferralType::kNoDeferral;

  std::queue<DelayedResponseTask> delayed_response_tasks_
      GUARDED_BY(response_queue_lock_);
  base::OnceClosure monitor_callback_ GUARDED_BY(response_queue_lock_);
  base::Lock response_queue_lock_;
};

class SearchPreloadDeferrableResponse final
    : public net::test_server::BasicHttpResponse {
 public:
  // Build a custom defined response that might be deferred based on
  // `deferral_type`. See the comment of `SearchPreloadTestResponseDeferralType`
  // for more information about the deferral type. Pass an empty string to
  // `response_body` if the response (note, not the header) should fail.
  SearchPreloadDeferrableResponse(
      SearchPreloadResponseController* test_harness,
      SearchPreloadTestResponseDeferralType deferral_type,
      net::HttpStatusCode code,
      base::StringPairs headers,
      const std::string& response_body);
  ~SearchPreloadDeferrableResponse() override;

  SearchPreloadDeferrableResponse(const SearchPreloadDeferrableResponse&) =
      delete;
  SearchPreloadDeferrableResponse& operator=(
      const SearchPreloadDeferrableResponse&) = delete;

  // net::test_server::BasicHttpResponse implementation.
  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override;

 private:
  // The test fixture that can manipulate the response.
  raw_ptr<SearchPreloadResponseController> test_harness_;

  // The deferral mode. See comment of `SearchPreloadTestResponseDeferralType`
  // for more information.
  const SearchPreloadTestResponseDeferralType service_deferral_type_ =
      SearchPreloadTestResponseDeferralType::kNoDeferral;

  // Predefined response headers.
  const base::StringPairs headers_;

  // Predefined response body. The response body will fail due to the
  // CONTENT_LENGTH_MISMATCH error if it is set to an empty string.
  const std::string body_ = "";
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PRELOAD_TEST_RESPONSE_UTILS_H_
