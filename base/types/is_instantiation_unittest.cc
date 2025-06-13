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

static_assert(is_instantiation<SingleArg<int>, SingleArg>);
static_assert(is_instantiation<SingleArg<char>, SingleArg>);
static_assert(is_instantiation<SingleArg<std::string>, SingleArg>);
static_assert(is_instantiation<SingleArg<std::vector<int>>, SingleArg>);

static_assert(!is_instantiation<int, SingleArg>);
static_assert(!is_instantiation<char, SingleArg>);
static_assert(!is_instantiation<std::vector<int>, SingleArg>);
static_assert(!is_instantiation<std::vector<SingleArg<int>>, SingleArg>);

static_assert(!is_instantiation<SingleArg<int>, std::vector>);

/////////////////////////////
// Variadic template
/////////////////////////////

template <typename...>
class Variadic;

static_assert(is_instantiation<Variadic<>, Variadic>);
static_assert(is_instantiation<Variadic<int>, Variadic>);
static_assert(is_instantiation<Variadic<int, char>, Variadic>);
static_assert(is_instantiation<Variadic<int, char, Variadic<>>, Variadic>);

static_assert(!is_instantiation<SingleArg<int>, Variadic>);
static_assert(!is_instantiation<Variadic<>, SingleArg>);
static_assert(!is_instantiation<Variadic<int>, SingleArg>);

/////////////////////////////
// Real types
/////////////////////////////

static_assert(is_instantiation<std::vector<bool>, std::vector>);
static_assert(is_instantiation<std::vector<int>, std::vector>);
static_assert(is_instantiation<std::map<int, char>, std::map>);

}  // namespace
}  // namespace base
