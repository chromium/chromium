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

void CreateTypeUint16() {
  constexpr size_t size = 64;
  uint16_t data[size];
  BufferIterator<uint16_t> iterator(data, size);  // expected-error@*:* {{Underlying buffer type must be char-type.}}
}

void ComplexMutableObject() {
  constexpr size_t size = 64;
  uint8_t data[size];
  BufferIterator<uint8_t> iterator(data, size);
  Complex* c = iterator.MutableObject<Complex>();  // expected-error {{no matching member function for call to 'MutableObject'}}
}

void ComplexObject() {
  constexpr size_t size = 64;
  uint8_t data[size];
  BufferIterator<uint8_t> iterator(data, size);
  const Complex* c = iterator.Object<Complex>();  // expected-error {{no matching member function for call to 'Object'}}
}

void ComplexMutableSpan() {
  constexpr size_t size = 64;
  uint8_t data[size];
  BufferIterator<uint8_t> iterator(data, size);
  base::span<Complex> s = iterator.MutableSpan<Complex>(3);  // expected-error {{no matching member function for call to 'MutableSpan'}}
}

void ComplexSpan() {
  constexpr size_t size = 64;
  uint8_t data[size];
  BufferIterator<uint8_t> iterator(data, size);
  base::span<const Complex> s = iterator.Span<Complex>();  // expected-error {{no matching member function for call to 'Span'}}
}

void OverflowCompileTime() {
  char buffer[64];

  BufferIterator<char> iterator(
      // SAFETY: This intentionally makes an incorrectly-sized span. The span
      // pointer, stored in the BufferIterator is never moved past the start in
      // this test, as that would cause Undefined Behaviour.
      UNSAFE_BUFFERS(span(buffer, std::numeric_limits<size_t>::max())));

  constexpr size_t kInvalidU64Size =
      (std::numeric_limits<size_t>::max() / sizeof(uint64_t)) + 1u;

  iterator.Span<uint64_t, kInvalidU64Size>(); // expected-error {{no matching member function}}
  iterator.Span<uint64_t, std::numeric_limits<size_t>::max()>();  // expected-error {{no matching member function}}
}

}  // namespace base
