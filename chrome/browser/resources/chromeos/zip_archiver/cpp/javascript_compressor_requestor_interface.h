// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_JAVASCRIPT_COMPRESSOR_REQUESTOR_INTERFACE_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_JAVASCRIPT_COMPRESSOR_REQUESTOR_INTERFACE_H_

#include <cstdint>

#include "ppapi/cpp/var_array_buffer.h"

// Makes requests from Compressor to JavaScript.
class JavaScriptCompressorRequestorInterface {
 public:
  virtual ~JavaScriptCompressorRequestorInterface() {}

  virtual void WriteChunkRequest(int64_t offset,
                                 int64_t length,
                                 const pp::VarArrayBuffer& buffer) = 0;

  virtual void ReadFileChunkRequest(int64_t length) = 0;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_JAVASCRIPT_COMPRESSOR_REQUESTOR_INTERFACE_H_
