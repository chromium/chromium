// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_URL_UTILS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_URL_UTILS_H_

#include <optional>
#include <string_view>

#include "url/gurl.h"

namespace ash {
namespace input_method {

// Checks if domain is a sub-domain of url
bool IsSubDomain(const GURL& url, std::string_view domain);

// Checks if url belongs to domain and has the path_prefix
bool IsSubDomainWithPathPrefix(const GURL& url,
                               std::string_view domain,
                               std::string_view path_prefix);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_URL_UTILS_H_
