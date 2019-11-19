// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class SafeBrowsingPrivateGetReferrerChainFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("safeBrowsingPrivate.getReferrerChain",
                             SAFEBROWSINGPRIVATE_GETREFERRERCHAIN)

  SafeBrowsingPrivateGetReferrerChainFunction();

 protected:
  ~SafeBrowsingPrivateGetReferrerChainFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPrivateGetReferrerChainFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_API_H_
