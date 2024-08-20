// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_CONVERSIONS_H_
#define CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_CONVERSIONS_H_

#include <memory>
#include <optional>

namespace net {
class CanonicalCookie;
}

namespace sync_pb {
class CookieSpecifics;
}

namespace ash::floating_sso {

// Returns nullptr if some members of `proto` can't be deserialized or
// if the cookie saved in `proto` is not canonical.
std::unique_ptr<net::CanonicalCookie> FromSyncProto(
    const sync_pb::CookieSpecifics& proto);

// Returns empty optional if some members of the `cookie` can't be serialized.
std::optional<sync_pb::CookieSpecifics> ToSyncProto(
    const net::CanonicalCookie& cookie);

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_CONVERSIONS_H_
