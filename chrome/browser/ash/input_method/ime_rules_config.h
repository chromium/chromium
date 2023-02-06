// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_IME_RULES_CONFIG_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_IME_RULES_CONFIG_H_

namespace ash {
namespace input_method {

// Runs the rule check against contextual info.
bool IsAutoCorrectDisabled(const TextFieldContextualInfo& info);

// Runs the rule check against url
bool IsMultiWordSuggestDisabled(const GURL& url);

// Checks if domain is a sub-domain of url
bool IsSubDomain(const GURL& url, const base::StringPiece domain);

// Checks if url belongs to domain and has the path_prefix
bool IsSubDomainWithPathPrefix(const GURL& url,
                               const base::StringPiece domain,
                               const base::StringPiece path_prefix);
}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_IME_RULES_CONFIG_H_
