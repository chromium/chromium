// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MACROS_IS_EMPTY_H_
#define BASE_MACROS_IS_EMPTY_H_

// A macro that substitutes with 1 if called without arguments, otherwise 0.
#define BASE_IS_EMPTY(...) BASE_INTERNAL_IS_EMPTY_EXPANDED(__VA_ARGS__)
#define BASE_INTERNAL_IS_EMPTY_EXPANDED(...) \
  BASE_INTERNAL_IS_EMPTY_INNER(_, ##__VA_ARGS__)
#define BASE_INTERNAL_IS_EMPTY_INNER(...) \
  BASE_INTERNAL_IS_EMPTY_INNER_EXPANDED(__VA_ARGS__, 0, 1)
#define BASE_INTERNAL_IS_EMPTY_INNER_EXPANDED(e0, e1, is_empty, ...) is_empty

#endif  // BASE_MACROS_IS_EMPTY_H_
