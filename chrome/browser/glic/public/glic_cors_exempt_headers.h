// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_CORS_EXEMPT_HEADERS_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_CORS_EXEMPT_HEADERS_H_

namespace network::mojom {
class NetworkContextParams;
}

namespace glic {

inline constexpr char kGlicHeaderName[] = "X-Glic";
inline constexpr char kGlicHeaderValue[] = "1";
inline constexpr char kGlicVersionHeaderName[] = "X-Glic-Chrome-Version";
inline constexpr char kGlicChannelHeaderName[] = "X-Glic-Chrome-Channel";

// Appends Glic custom request header names to the CORS exempt header list in
// `params`.
void UpdateCorsExemptHeaders(network::mojom::NetworkContextParams* params);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_CORS_EXEMPT_HEADERS_H_
