// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_TWO_PHASE_UPLOADER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_TWO_PHASE_UPLOADER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class TaskRunner;
}
namespace network {
class SharedURLLoaderFactory;
}

class TwoPhaseUploaderFactory;

// Implements the Google two-phase resumable upload protocol.
// Protocol documentation:
// https://developers.google.com/storage/docs/developer-guide#resumable
// Note: This doc is for the Cloud Storage API which specifies the POST body
// must be empty, however the safebrowsing use of the two-phase protocol
// supports sending metadata in the POST request body. We also do not need the
// api-version and authorization headers.
// TODO(mattm): support retry / resume.
// Lives on the UI thread.
class TwoPhaseUploader {
 public:
  enum State {
    STATE_NONE,
    UPLOAD_METADATA,
    UPLOAD_FILE,
    STATE_SUCCESS,
  };
  using FinishCallback =
      base::OnceCallback<void(State state,
                              int net_error,
                              int response_code,
                              const std::string& response_data)>;

  virtual ~TwoPhaseUploader() {}

  // Create the uploader.  The Start method must be called to begin the upload.
  // Network processing will use |url_request_context_getter|.
  // The uploaded |file_path| will be read on |file_task_runner|.
  // The first phase request will be sent to |base_url|, with |metadata|
  // included.
  // On success |finish_callback| will be called with state = STATE_SUCCESS and
  // the server response in response_data. On failure, state will specify
  // which step the failure occurred in, and net_error, response_code, and
  // response_data will specify information about the error. |finish_callback|
  // will not be called if the upload is cancelled by destructing the
  // TwoPhaseUploader object before completion.
  static std::unique_ptr<TwoPhaseUploader> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::TaskRunner* file_task_runner,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file_path,
      FinishCallback finish_callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Makes the passed |factory| the factory used to instantiate
  // a TwoPhaseUploader. Useful for tests.
  static void RegisterFactory(TwoPhaseUploaderFactory* factory) {
    factory_ = factory;
  }

  // Begins the upload process.
  virtual void Start() = 0;

 private:
  // The factory that controls the creation of SafeBrowsingProtocolManager.
  // This is used by tests.
  static TwoPhaseUploaderFactory* factory_;
};

class TwoPhaseUploaderFactory {
 public:
  virtual ~TwoPhaseUploaderFactory() {}

  virtual std::unique_ptr<TwoPhaseUploader> CreateTwoPhaseUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::TaskRunner* file_task_runner,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file_path,
      TwoPhaseUploader::FinishCallback finish_callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_TWO_PHASE_UPLOADER_H_
