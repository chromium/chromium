// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/is_instantiation.h"

#include <map>
#include <string>
#include <vector>

namespace base {
namespace {

/////////////////////////////
// Single-argument template
/////////////////////////////

template <typename T1>
class SingleArg;

static_assert(is_instantiation<SingleArg, SingleArg<int>>);
static_assert(is_instantiation<SingleArg, SingleArg<char>>);
static_assert(is_instantiation<SingleArg, SingleArg<std::string>>);
static_assert(is_instantiation<SingleArg, SingleArg<std::vector<int>>>);

static_assert(!is_instantiation<SingleArg, int>);
static_assert(!is_instantiation<SingleArg, char>);
static_assert(!is_instantiation<SingleArg, std::vector<int>>);
static_assert(!is_instantiation<SingleArg, std::vector<SingleArg<int>>>);

static_assert(!is_instantiation<std::vector, SingleArg<int>>);

/////////////////////////////
// Variadic template
/////////////////////////////

template <typename...>
class Variadic;

static_assert(is_instantiation<Variadic, Variadic<>>);
static_assert(is_instantiation<Variadic, Variadic<int>>);
static_assert(is_instantiation<Variadic, Variadic<int, char>>);
static_assert(is_instantiation<Variadic, Variadic<int, char, Variadic<>>>);

static_assert(!is_instantiation<Variadic, SingleArg<int>>);
static_assert(!is_instantiation<SingleArg, Variadic<>>);
static_assert(!is_instantiation<SingleArg, Variadic<int>>);

/////////////////////////////
// Real types
/////////////////////////////

static_assert(is_instantiation<std::vector, std::vector<bool>>);
static_assert(is_instantiation<std::vector, std::vector<int>>);
static_assert(is_instantiation<std::map, std::map<int, char>>);

}  // namespace
}  // namespace base
