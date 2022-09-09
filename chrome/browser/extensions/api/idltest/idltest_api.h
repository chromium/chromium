// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDLTEST_IDLTEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDLTEST_IDLTEST_API_H_

#include "extensions/browser/extension_function.h"

class IdltestSendArrayBufferFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idltest.sendArrayBuffer", IDLTEST_SENDARRAYBUFFER)

 protected:
  ~IdltestSendArrayBufferFunction() override {}
  ResponseAction Run() override;
};

class IdltestSendArrayBufferViewFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idltest.sendArrayBufferView",
                             IDLTEST_SENDARRAYBUFFERVIEW)

 protected:
  ~IdltestSendArrayBufferViewFunction() override {}
  ResponseAction Run() override;
};

class IdltestGetArrayBufferFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idltest.getArrayBuffer", IDLTEST_GETARRAYBUFFER)

 protected:
  ~IdltestGetArrayBufferFunction() override {}
  ResponseAction Run() override;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDLTEST_IDLTEST_API_H_
