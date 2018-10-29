// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_JAVASCRIPT_REQUESTOR_INTERFACE_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_JAVASCRIPT_REQUESTOR_INTERFACE_H_

#include <cstdint>
#include <string>

// Makes requests to JavaScript. Requests are asynchronous and responses must be
// handled by other classes. This class stricly makes requests.
class JavaScriptRequestorInterface {
 public:
  virtual ~JavaScriptRequestorInterface() {}

  // Request a file chunk from JavaScript. The request is asynchronous.
  // offset must be >= 0 and bytes_to_read > 0.
  virtual void RequestFileChunk(const std::string& request_id,
                                int64_t offset,
                                int64_t bytes_to_read) = 0;

  // Request a passphrase from JavaScript. The request is asynchronous.
  virtual void RequestPassphrase(const std::string& request_id) = 0;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_JAVASCRIPT_REQUESTOR_INTERFACE_H_
