// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/canonical_cookie_hash.h"

#include "base/hash/hash.h"

namespace canonical_cookie {

size_t FastHash(const net::CanonicalCookie& cookie) {
  return base::PersistentHash(cookie.Name()) +
         3 * base::PersistentHash(cookie.Domain()) +
         7 * base::PersistentHash(cookie.Path());
}

}  // namespace canonical_cookie
