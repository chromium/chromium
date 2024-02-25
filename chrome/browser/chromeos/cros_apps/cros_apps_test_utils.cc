// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/cros_apps_test_utils.h"

#include "base/strings/stringprintf.h"

content::EvalJsResult IsIdentifierDefined(
    const content::ToRenderFrameHost& to_rfh,
    const char* identifier) {
  return content::EvalJs(
      to_rfh, base::StringPrintf("typeof %s !== 'undefined';", identifier));
}
