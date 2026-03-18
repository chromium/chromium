// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/authenticated_network_fetcher.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/update_client/network.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace extensions {

namespace {

std::optional<GURL> MaybeGetRetryURL(const GURL& url, int response_code) {
  if (!url.DomainIs("google.com")) {
    return std::nullopt;
  }
  if (response_code != net::HTTP_UNAUTHORIZED &&
      response_code != net::HTTP_FORBIDDEN) {
    return std::nullopt;
  }

  static constexpr std::string_view kAuthUserQueryKey = "authuser";

  // The maximum number of users to attempt authentication as.
  static constexpr int kMaxAuthUserValue = 10;

  const int user_index = [&url] {
    std::string user_index_str;
    if (!net::GetValueForKeyInQuery(url, kAuthUserQueryKey, &user_index_str)) {
      return 0;
    }
    int user_index = 0;
    if (!base::StringToInt(user_index_str, &user_index)) {
      return 0;
    }
    return user_index;
  }();
  if (user_index >= kMaxAuthUserValue) {
    return std::nullopt;
  }
  return net::AppendOrReplaceQueryParameter(
      url, kAuthUserQueryKey, base::NumberToString(user_index + 1));
}

// Returns a callback which retains `ptr` until invoked or dropped. This is
// useful for creating "self-owned" NetworkFetchers which live as long as their
// request.
template <typename T>
base::OnceClosure Retain(std::unique_ptr<T> ptr) {
  return base::BindOnce([](std::unique_ptr<T>) {}, std::move(ptr));
}

}  // namespace

class AuthenticatedNetworkFetcher::DownloadToFileState
    : public base::RefCountedThreadSafe<DownloadToFileState> {
 public:
  void OnRequestStart(base::OnceClosure cancel) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    response_code_ = -1;
    cancel_ = std::move(cancel);
  }

  void OnResponseStart(int response_code) { response_code_ = response_code; }

  void Cancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (cancel_) {
      std::move(cancel_).Run();
    }
  }

  int response_code() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return response_code_;
  }

 private:
  friend class base::RefCountedThreadSafe<DownloadToFileState>;
  ~DownloadToFileState() = default;

  SEQUENCE_CHECKER(sequence_checker_);
  int response_code_ = -1;
  base::OnceClosure cancel_;
};

AuthenticatedNetworkFetcher::AuthenticatedNetworkFetcher(
    scoped_refptr<update_client::NetworkFetcherFactory> base_factory)
    : base_factory_(base_factory) {}

AuthenticatedNetworkFetcher::~AuthenticatedNetworkFetcher() = default;

void AuthenticatedNetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<update_client::NetworkFetcher> fetcher =
      base_factory_->Create();
  fetcher->PostRequest(url, post_data, content_type, post_additional_headers,
                       std::move(response_started_callback),
                       std::move(progress_callback),
                       std::move(post_request_complete_callback)
                           .Then(Retain(std::move(fetcher))));
}

base::OnceClosure AuthenticatedNetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<update_client::NetworkFetcher> fetcher =
      base_factory_->Create();
  auto state = base::MakeRefCounted<DownloadToFileState>();
  state->OnRequestStart(fetcher->DownloadToFile(
      url, file_path,
      base::BindRepeating(
          &AuthenticatedNetworkFetcher::OnDownloadResponseStarted,
          weak_ptr_factory_.GetWeakPtr(), url, response_started_callback,
          state),
      progress_callback,
      base::BindOnce(&AuthenticatedNetworkFetcher::OnDownloadToFileComplete,
                     weak_ptr_factory_.GetWeakPtr(), url, file_path,
                     response_started_callback, progress_callback,
                     std::move(download_to_file_complete_callback)
                         .Then(Retain(std::move(fetcher))),
                     state)));
  return base::BindPostTaskToCurrentDefault(
      base::BindOnce(&DownloadToFileState::Cancel, state));
}

void AuthenticatedNetworkFetcher::OnDownloadResponseStarted(
    const GURL& url,
    ResponseStartedCallback callback,
    scoped_refptr<DownloadToFileState> state,
    int response_code,
    int64_t content_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state->OnResponseStart(response_code);

  // Avoid signaling the start of the download if it will be retried.
  if (!MaybeGetRetryURL(url, response_code).has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(callback, response_code, content_length));
  }
}

void AuthenticatedNetworkFetcher::OnDownloadToFileComplete(
    GURL url,
    base::FilePath file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback,
    scoped_refptr<DownloadToFileState> state,
    int net_error,
    int64_t content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<GURL> next_url = MaybeGetRetryURL(url, state->response_code());
  if (next_url) {
    std::unique_ptr<update_client::NetworkFetcher> fetcher =
        base_factory_->Create();
    state->OnRequestStart(fetcher->DownloadToFile(
        *next_url, file_path,
        base::BindRepeating(
            &AuthenticatedNetworkFetcher::OnDownloadResponseStarted,
            weak_ptr_factory_.GetWeakPtr(), *next_url,
            response_started_callback, state),
        progress_callback,
        base::BindOnce(&AuthenticatedNetworkFetcher::OnDownloadToFileComplete,
                       weak_ptr_factory_.GetWeakPtr(), *next_url, file_path,
                       response_started_callback, progress_callback,
                       std::move(download_to_file_complete_callback)
                           .Then(Retain(std::move(fetcher))),
                       state)));
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(download_to_file_complete_callback),
                                net_error, content_size));
}

AuthenticatedNetworkFetcherFactory::AuthenticatedNetworkFetcherFactory(
    scoped_refptr<update_client::NetworkFetcherFactory> base_factory)
    : base_factory_(base_factory) {}

AuthenticatedNetworkFetcherFactory::~AuthenticatedNetworkFetcherFactory() =
    default;

std::unique_ptr<update_client::NetworkFetcher>
AuthenticatedNetworkFetcherFactory::Create() const {
  return std::make_unique<AuthenticatedNetworkFetcher>(base_factory_);
}

}  // namespace extensions
