// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_HSTRING_REFERENCE_H_
#define BASE_WIN_HSTRING_REFERENCE_H_

#include <hstring.h>

#include "base/base_export.h"

namespace base {
namespace win {

// HStringReference is an HSTRING representation of a null terminated
// string backed by memory that outlives the HStringReference instance.
//
// If you need an HSTRING class that manages its own memory, you should
// use ScopedHString instead.
//
// Note that HStringReference requires certain functions that are only
// available on Windows 8 and later, and that these functions need to be
// delayloaded to avoid breaking Chrome on Windows 7.
//
// Callers MUST check the return value of ResolveCoreWinRTStringDelayLoad()
// *before* using HStringReference.
//
// One-time Initialization for HStringReference:
//
//   const bool success = HStringReference::ResolveCoreWinRTStringDelayload();
//   if (success) {
//     // HStringReference can be used.
//   } else {
//     // Handle error.
//   }
//
// Example use:
//
//   HStringReference string(L"abc");
//
class BASE_EXPORT HStringReference {
 public:
  // Loads all required HSTRING functions, available from Win8 and onwards.
  static bool ResolveCoreWinRTStringDelayload();

  HStringReference(const wchar_t* str, size_t len);
  explicit HStringReference(const wchar_t* str);

  HSTRING Get() const { return hstring_; }

  // HSTRING_HEADER is a structure that contains a pointer to the string
  // passed into the constructor, along with its length.

  // Since HSTRING is a pointer to HSTRING_HEADER, HStringReference
  // cannot be copyable, moveable or assignable, as that would invalidate
  // the HSTRING we're passing out to clients.

  // In the future, we can consider implementing these methods by storing
  // the string passed in the constructor and re-creating the HSTRING and
  // HSTRING_HEADER datastructures. For now, we'll keep things simple and
  // forbid these operations.
  HStringReference(const HStringReference&) = delete;
  HStringReference& operator=(const HStringReference&) = delete;

 private:
  HSTRING hstring_ = nullptr;
  HSTRING_HEADER hstring_header_;
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_HSTRING_REFERENCE_H_