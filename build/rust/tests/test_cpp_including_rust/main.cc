// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defined in Rust.
extern "C" void say_hello_from_cpp();

int main(int argc, char* argv[]) {
  say_hello_from_cpp();
  return 0;
}
