// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_FEEDBACK_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_FEEDBACK_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

class DownloadFeedbackFactory;

// Handles the uploading of a single downloaded binary to the safebrowsing
// download feedback service.
class DownloadFeedback {
 public:
  // Takes ownership of the file pointed to be |file_path|, it will be deleted
  // when the DownloadFeedback is destructed.
  static std::unique_ptr<DownloadFeedback> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& file_path,
      uint64_t file_size,
      const std::string& ping_request,
      const std::string& ping_response);

  // The largest file size we support uploading.
  // Note: changing this will affect the max size of
  // SBDownloadFeedback.SizeSuccess and SizeFailure histograms.
  static const int64_t kMaxUploadSize;

  // The URL where the browser sends download feedback requests.
  static const char kSbFeedbackURL[];

  virtual ~DownloadFeedback() {}

  // Makes the passed |factory| the factory used to instantiate
  // a DownloadFeedback. Useful for tests.
  static void RegisterFactory(DownloadFeedbackFactory* factory) {
    factory_ = factory;
  }

  // Start uploading the file to the download feedback service.
  // |finish_callback| will be called when the upload completes or fails, but is
  // not called if the upload is cancelled by deleting the DownloadFeedback
  // while the upload is in progress.
  virtual void Start(base::OnceClosure finish_callback) = 0;

  virtual const std::string& GetPingRequestForTesting() const = 0;
  virtual const std::string& GetPingResponseForTesting() const = 0;

 private:
  // The factory that controls the creation of DownloadFeedback objects.
  // This is used by tests.
  static DownloadFeedbackFactory* factory_;
};

class DownloadFeedbackFactory {
 public:
  virtual ~DownloadFeedbackFactory() {}

  virtual std::unique_ptr<DownloadFeedback> CreateDownloadFeedback(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& file_path,
      uint64_t file_size,
      const std::string& ping_request,
      const std::string& ping_response) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_FEEDBACK_H_
