// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mac:
// clang -Os -isysroot $(xcrun -show-sdk-path) -mmacosx-version-min=10.10 \
//     -arch x86_64 print_arch.c -o x86_64 -Wl,-s -Wl,-dead_strip
// clang -Os -isysroot $(xcrun -show-sdk-path) -mmacosx-version-min=11.0 \
//     -arch arm64 print_arch.c -o arm64 -Wl,-s -Wl,-dead_strip
// lipo -create x86_64 arm64 -o universal
// clang -Os -isysroot /apple/sdk/MacOSX10.13.sdk -mmacosx-version-min=10.10 \
//     -arch i386 print_arch.c -o x86 -Wl,-s -Wl,-dead_strip
//
// Linux:
// gcc -Os -fuse-ld=gold print_arch.c -o elf -s \
//     -ffunction-sections -fdata-sections -Wl,--gc-sections

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
#if defined(__i386__)
  static const char kArchitecture[] = "x86";
#elif defined(__x86_64__)
  static const char kArchitecture[] = "x86_64";
#elif defined(__arm64__) || defined(__aarch64__)
  static const char kArchitecture[] = "arm64";
#endif

  printf("%s\n", kArchitecture);

  return EXIT_SUCCESS;
}
