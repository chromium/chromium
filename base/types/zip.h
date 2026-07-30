// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TYPES_ZIP_H_
#define BASE_TYPES_ZIP_H_

#include <ranges>

namespace base {

// TODO(crbug.com/493534962): Migrate callers and remove this completely.
inline constexpr auto zip = std::views::zip;

}  // namespace base

#endif  // BASE_TYPES_ZIP_H_
