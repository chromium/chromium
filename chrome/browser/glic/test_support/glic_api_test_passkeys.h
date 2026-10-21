// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_API_TEST_PASSKEYS_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_API_TEST_PASSKEYS_H_

#include "chrome/browser/glic/public/glic_passkeys.h"

namespace glic {

class GlicApiTestPasskeys {
 public:
  static InvokeWithAutoSubmitPasskey GetPassKey();
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_API_TEST_PASSKEYS_H_
