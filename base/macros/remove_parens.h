// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MACROS_REMOVE_PARENS_H_
#define BASE_MACROS_REMOVE_PARENS_H_

#include "base/macros/if.h"

// A macro that removes at most one outer set of parentheses from its arguments.
// If the arguments are not surrounded by parentheses, this expands to the
// arguments unchanged. For example:
// `BASE_REMOVE_PARENS()` -> ``
// `BASE_REMOVE_PARENS(foo)` -> `foo`
// `BASE_REMOVE_PARENS(foo(1))` -> `foo(1)`
// `BASE_REMOVE_PARENS((foo))` -> `foo`
// `BASE_REMOVE_PARENS((foo(1)))` -> `foo(1)`
// `BASE_REMOVE_PARENS((foo)[1])` -> `(foo)[1]`
// `BASE_REMOVE_PARENS(((foo)))` -> `(foo)`
// `BASE_REMOVE_PARENS(foo, bar, baz)` -> `foo, bar, baz`
// `BASE_REMOVE_PARENS(foo, (bar), baz)` -> `foo, (bar), baz`
#define BASE_REMOVE_PARENS(...)                                            \
  BASE_IF(BASE_INTERNAL_IS_PARENTHESIZED(__VA_ARGS__), BASE_INTERNAL_ECHO, \
          BASE_INTERNAL_EMPTY())                                           \
  __VA_ARGS__

#define BASE_INTERNAL_IS_PARENTHESIZED(...) \
  BASE_INTERNAL_IS_EMPTY(BASE_INTERNAL_EAT __VA_ARGS__)
#define BASE_INTERNAL_IS_EMPTY(...) BASE_INTERNAL_IS_EMPTY_EXPANDED(__VA_ARGS__)
#define BASE_INTERNAL_IS_EMPTY_EXPANDED(...) \
  BASE_INTERNAL_IS_EMPTY_INNER(_, ##__VA_ARGS__)
#define BASE_INTERNAL_IS_EMPTY_INNER(...) \
  BASE_INTERNAL_IS_EMPTY_INNER_EXPANDED(__VA_ARGS__, 0, 1)
#define BASE_INTERNAL_IS_EMPTY_INNER_EXPANDED(e0, e1, is_empty, ...) is_empty
#define BASE_INTERNAL_EAT(...)
#define BASE_INTERNAL_ECHO(...) __VA_ARGS__
#define BASE_INTERNAL_EMPTY()

#endif  // BASE_MACROS_REMOVE_PARENS_H_
