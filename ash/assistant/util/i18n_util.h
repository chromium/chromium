// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_I18N_UTIL_H_
#define ASH_ASSISTANT_UTIL_I18N_UTIL_H_

#include <string>

#include "base/component_export.h"

class GURL;

namespace ash {
namespace assistant {
namespace util {

// Returns a GURL for the specified |url| having set the locale query parameter.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL CreateLocalizedGURL(const std::string& url);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_I18N_UTIL_H_
