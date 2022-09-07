// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/buffer_iterator.h"

#include <stdint.h>

#include <string>

namespace base {

class Complex {
 public:
  Complex() : string_("Moo") {}

 private:
  std::string string_;
};

#if defined(NCTEST_BUFFER_ITERATOR_CREATE_TYPE_UINT16)  // [r"fatal error: static_assert failed due to requirement 'std::is_same<unsigned short, char>::value || std::is_same<unsigned short, unsigned char>::value': Underlying buffer type must be char-type."]

void WontCompile() {
  constexpr size_t size = 64;
  uint16_t data[size];
  BufferIterator<uint16_t> iterator(data, size);
}

#elif defined(NCTEST_BUFFER_ITERATOR_COMPLEX_MUTABLE_OBJECT)  // [r"no matching member function for call to 'MutableObject'"]

void WontCompile() {
  constexpr size_t size = 64;
  uint8_t data[size];
  BufferIterator<uint8_t> iterator(data, size);
  Complex* c = iterator.MutableObject<Complex>();
}

#elif defined(NCTEST_BUFFER_ITERATOR_COMPLEX_OBJECT)  // [r"no matching member function for call to 'Object'"]

void WontCompile() {
  constexpr size_t size = 64;
  uint8_t data[size];
  BufferIterator<uint8_t> iterator(data, size);
  const Complex* c = iterator.Object<Complex>();
}

#elif defined(NCTEST_BUFFER_ITERATOR_COMPLEX_MUTABLE_SPAN)  // [r"no matching member function for call to 'MutableSpan'"]

void WontCompile() {
  constexpr size_t size = 64;
  uint8_t data[size];
  BufferIterator<uint8_t> iterator(data, size);
  base::span<Complex> s = iterator.MutableSpan<Complex>(3);
}

#elif defined(NCTEST_BUFFER_ITERATOR_COMPLEX_SPAN)  // [r"no matching member function for call to 'Span'"]

void WontCompile() {
  constexpr size_t size = 64;
  uint8_t data[size];
  BufferIterator<uint8_t> iterator(data, size);
  base::span<const Complex> s = iterator.Span<Complex>();
}

#endif

}  // namespace base
