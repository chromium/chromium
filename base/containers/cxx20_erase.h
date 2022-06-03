// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CXX20_ERASE_H_
#define BASE_CONTAINERS_CXX20_ERASE_H_

#include "base/containers/cxx20_erase_deque.h"
#include "base/containers/cxx20_erase_forward_list.h"
#include "base/containers/cxx20_erase_list.h"
#include "base/containers/cxx20_erase_map.h"
#include "base/containers/cxx20_erase_set.h"
#include "base/containers/cxx20_erase_string.h"
#include "base/containers/cxx20_erase_unordered_map.h"
#include "base/containers/cxx20_erase_unordered_set.h"
#include "base/containers/cxx20_erase_vector.h"

// Erase/EraseIf are based on C++20's uniform container erasure API:
// - https://eel.is/c++draft/libraryindex#:erase
// - https://eel.is/c++draft/libraryindex#:erase_if
// They provide a generic way to erase elements from a container.
// The functions here implement these for the standard containers until those
// functions are available in the C++ standard.
// Note: there is no std::erase for standard associative containers so we don't
// have it either.

// This header is provided for convenience, so callers to Erase/EraseIf can just
// include this in their .cc file without thinking about which Erase/EraseIf
// specialization header to include. For uncommon cases where Erase/EraseIf are
// used in .h files, please include the specialization header to avoid bloating
// the header.

#endif  // BASE_CONTAINERS_CXX20_ERASE_H_
