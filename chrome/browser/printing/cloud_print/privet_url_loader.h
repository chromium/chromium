// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_URL_LOADER_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_URL_LOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace cloud_print {

// Privet-specific URLLoader adapter. Currently supports only the subset of
// HTTP features required by Privet for GCP 1.5 (/privet/info and
// /privet/register).
class PrivetURLLoader {
 public:
  enum ErrorType {
    JSON_PARSE_ERROR,
    REQUEST_CANCELED,
    RESPONSE_CODE_ERROR,
    TOKEN_ERROR,
    UNKNOWN_ERROR,
  };

  using TokenCallback = base::OnceCallback<void(const std::string& /*token*/)>;

  class Delegate {
   public:
    virtual ~Delegate() {}

    // If you do not implement this method for PrivetV1 callers, you will always
    // get a TOKEN_ERROR error when your token is invalid.
    virtual void OnNeedPrivetToken(TokenCallback callback);

    // |response_code| is only needed for RESPONSE_CODE_ERROR.
    virtual void OnError(int response_code, ErrorType error) = 0;
    virtual void OnParsedJson(int response_code,
                              const base::DictionaryValue& value,
                              bool has_error) = 0;

    // If this method returns true, the data will not be parsed as JSON, and
    // OnParsedJson() will not be called. Otherwise, OnParsedJson() will be
    // called. This only happens in tests.
    virtual bool OnRawData(bool response_is_file,
                           const std::string& data_string,
                           const base::FilePath& data_file);
  };

  PrivetURLLoader(
      const GURL& url,
      const std::string& request_type,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Delegate* delegate);

  virtual ~PrivetURLLoader();

  static void SetTokenForHost(const std::string& host,
                              const std::string& token);

  static void ResetTokenMapForTest();

  void SetMaxRetriesForTest(int max_retries);

  void DoNotRetryOnTransientError();

  void SendEmptyPrivetToken();

  // Set the contents of the Range header. OnRawData() must return true if this
  // is called.
  void SetByteRange(int start, int end);

  // Save the response to a file. OnRawData() must return true if this is
  // called.
  void SaveResponseToFile();

  void Start();

  void SetUploadData(const std::string& upload_content_type,
                     const std::string& upload_data);

  // A class that can be used in tests that want to bypass the delay when
  // retrying loading a URL. Create one of this object in your test, it will
  // disable the delay for retry on construction and revert it back on
  // destruction.
  // Note that you should not have more than one of these allocated at a time.
  class RetryImmediatelyForTest final {
   public:
    RetryImmediatelyForTest();
    ~RetryImmediatelyForTest();
  };

 private:
  std::string GetHostString();  // Get string representing the host.
  std::string GetPrivetAccessToken();
  void Try();
  void ScheduleRetry(int timeout_seconds);
  bool PrivetErrorTransient(const std::string& error);
  void RequestTokenRefresh();
  void RefreshToken(const std::string& token);
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);
  void OnDownloadedToString(std::unique_ptr<std::string> response_body);
  void OnDownloadedToFile(base::FilePath path);
  bool CheckURLLoaderForError();

  const GURL url_;
  const std::string request_type_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  Delegate* const delegate_;

  int max_retries_;
  bool do_not_retry_on_transient_error_ = false;
  bool send_empty_privet_token_ = false;
  bool has_byte_range_ = false;
  bool make_response_file_ = false;
  static bool skip_retry_timeouts_for_tests_;

  int byte_range_start_ = 0;
  int byte_range_end_ = 0;

  int tries_ = 0;
  std::string upload_data_;
  std::string upload_content_type_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<PrivetURLLoader> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PrivetURLLoader);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_URL_LOADER_H_
