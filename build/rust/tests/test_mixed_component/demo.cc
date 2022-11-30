// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include "component.h"

int main(int argc, const char* argv[]) {
  // This doesn't really test anything except that everything links
  // OK.
  std::cout << "1 + 1 = " << bilingual_math(1, 1) << std::endl;
  return 0;
}
