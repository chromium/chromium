// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_IME_RULES_CONFIG_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_IME_RULES_CONFIG_H_

#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

// Runs the rule check against contextual info.
bool IsAssistiveInputDisabled(const absl::optional<GURL>& current_url);

// Checks if domain is a sub-domain of url
bool IsSubDomain(const GURL& url, const base::StringPiece domain);

// Checks if url belongs to domain and has the path_prefix
bool IsSubDomainWithPathPrefix(const GURL& url,
                               const base::StringPiece domain,
                               const base::StringPiece path_prefix);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_IME_RULES_CONFIG_H_
