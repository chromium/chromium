// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_api_test_passkeys.h"

namespace glic {

// static
InvokeWithAutoSubmitPasskey GlicApiTestPasskeys::GetPassKey() {
  return InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
}

}  // namespace glic
