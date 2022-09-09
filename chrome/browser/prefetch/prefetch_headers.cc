// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_headers.h"

namespace prefetch::headers {

// The header used to indicate the purpose of a request. It should have one of
// the two values below.
const char kSecPurposeHeaderName[] = "Sec-Purpose";

// This value indicates that the request is a prefetch request made directly to
// the server.
const char kSecPurposePrefetchHeaderValue[] = "prefetch";

// This value indicates that the request is a prefetch request made via an
// anonymous client IP proxy.
const char kSecPurposePrefetchAnonymousClientIpHeaderValue[] =
    "prefetch;anonymous-client-ip";

}  // namespace prefetch::headers
