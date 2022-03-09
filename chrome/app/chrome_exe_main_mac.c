// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entry point for all Mac Chromium processes, including the outer app
// bundle (browser) and helper app (renderer, plugin, and friends).

#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version.h"
#include "content/public/app/aperitif_mac.h"

typedef int (*ChromeMainPtr)(int, char**);

#if !defined(HELPER_EXECUTABLE) && defined(OFFICIAL_BUILD) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(ARCH_CPU_X86_64)
// This is for https://crbug.com/1300598, and more generally,
// https://crbug.com/1297588 (and all of the associated bugs). It's horrible!
//
// When the main executable is updated on disk while the application is running,
// and the offset of the Mach-O image at the main executable's path changes from
// the offset that was determined when the executable was loaded, SecCode ceases
// to be able to work with the executable. This may be triggered when the
// product is updated on disk but the application has not yet relaunched. This
// affects SecCodeCopySelf and SecCodeCopyGuestWithAttributes. Bugs are evident
// even when validation (SecCodeCheckValidity) is not attempted.
//
// Practically, this is only a concern for fat (universal) files, because the
// offset of a Mach-O image in a thin (single-architecture) file is always 0.
// The branded product always ships a fat executable, and because some uses of
// SecCode are in OS code beyond Chrome's control, an effort is made to freeze
// the geometry of the branded (BUILDFLAG(GOOGLE_CHROME_BRANDING))
// for-public-release (defined(OFFICIAL_BUILD)) main executable.
//
// The fat file is produced by installer/mac/universalizer.py. The x86_64 slice
// always precedes the arm64 slice: lipo, as used by universalizer.py, always
// places the arm64 slice last. See Xcode 12.0
// https://github.com/apple-oss-distributions/cctools/blob/cctools-973.0.1/misc/lipo.c#L2672
// cmp_qsort, used by create_fat at #L962. universalizer.py ensures that the
// first slice in the file is located at a constant offset (16kB since
// 98.0.4758.80), but if the first slice's size changes, it can affect the
// offset of the second slice, the arm64 one, triggering SecCode-related bugs
// for arm64 users across updates.
//
// As quite a hack of a workaround, the offset of the arm64 slice within the fat
// main executable is influenced to land at the desired location by introducing
// padding to the x86_64 slice that precedes it. The arm64 slice needs to remain
// at offset 304kB (since 98.0.4758.80). The signed x86_64 slice has size 287296
// bytes in 98.0.4758.80, but has shrunk since then, and before the introduction
// of any padding, would now be 37724 bytes long. To make up the 249572-byte
// difference, 240kB (245760 bytes) of padding is added to the x86_64 slice to
// ensure that its size is stable, causing the arm64 slice to land where it
// needs to be when universalized. This padding needs to be added to the thin
// form of the x86_64 image before being fed to universalizer.py. Why 245760
// bytes and not 249572? To keep it an even multiple of linker pages (not
// machine pages: linker pages are 4kB for lld targeting x86_64 and 16kB for
// ld64 targeting x86_64, but Chrome uses lld). In any case, I'll make up almost
// all of the 3812-byte difference with one more weird trick below.
//
// There are several terrible ways to insert this padding into the x86_64 image.
// Best would be something that considers the size of the x86_64 image without
// padding, and inserts the precise amount required. It may be possible to do
// this after linking, but the options that have been attempted so far were not
// successful. So this quick and very dirty 240kB buffer is added to increase
// the size of __TEXT,__const in a way that no tool could possibly see as
// suspicious after link time. The variable is marked with the "used" attribute
// to prevent the compiler from issuing warnings about the referenced variable,
// to prevent the compiler from removing it under optimization, and to set the
// S_ATTR_NO_DEAD_STRIP section attribute to prevent the linker from removing it
// under -dead_strip. Note that the standardized [[maybe_unused]] attribute only
// suppresses the warning, but does not prevent the compiler or linker from
// removing it.
//
// The introduction of this fixed 240kB of padding causes the unsigned linker
// output to grow by 240kB precisely, but the signed output will grow by
// slightly more. This is because the code signature's code directory contains
// SHA-1 and SHA-256 hashes of each 4kB code signing page (note, not machine
// pages or linker pages) in the image, adding 20 and 32 bytes each (macOS
// 12.0.1
// https://github.com/apple-oss-distributions/Security/blob/main/OSX/libsecurity_codesigning/lib/signer.cpp#L298
// Security::CodeSigning::SecCodeSigner::Signer::prepare). For the 240kB
// addition, the code signature grows by (240 / 4) * (20 + 32) = 3120 bytes,
// thus the total size of the linker output grows by 240kB + 3120 = 248880
// bytes. It is not possible to control this any more granularly: if the buffer
// were sized at 240kB - 3120 = 242640 bytes, it would either cause no change in
// the space allocated to the __TEXT segment (due to padding for alignment) or
// would cause the segment to shrink by a linker page (note, not a code signing
// or machine page) which would which would cause the linker output to shrink by
// the same amount and would be absolutely undesirable. Luckily, the net growth
// of 248880 bytes is very close to the target growth of 249572 bytes. In any
// event, having the signed x86_64 slice sized at 286604 bytes instead of 287296
// should not be a problem. Subtle differences in characteristics including the
// code signature itself can easily produce differences of that magnitude. It's
// necessary for the size to wind up in the range (278528, 294912], and as long
// as that's met, the 16kB alignment for the arm64 slice that follows it in the
// fat file will cause it to appear at the desired 304kB.
//
// If the main executable has a significant change in size, this will need to be
// revised. Hopefully a more elegant solution will become apparent before that's
// required.
static __attribute__((used))
const char kGrossPaddingForCrbug1300598[240 * 1024] = {};
#endif

__attribute__((visibility("default"))) int main(int argc, char* argv[]) {
  AperitifInitializePartitionAlloc();

  char exec_path[PATH_MAX];
  uint32_t exec_path_size = sizeof(exec_path);
  int rv = _NSGetExecutablePath(exec_path, &exec_path_size);
  if (rv != 0) {
    AperitifFatalError("_NSGetExecutablePath: get path failed.");
  }

#if defined(HELPER_EXECUTABLE)
  // Start the sandbox before loading the framework.
  AperitifInitializeSandbox(exec_path, argc, (const char**)argv);

  // The helper lives within the versioned framework directory, so simply
  // go up to find the main dylib.
  static const char rel_path[] =
      "../../../../" PRODUCT_FULLNAME_STRING " Framework";
#else
  static const char rel_path[] =
      "../Frameworks/" PRODUCT_FULLNAME_STRING
      " Framework.framework/Versions/" CHROME_VERSION_STRING
      "/" PRODUCT_FULLNAME_STRING " Framework";
#endif  // defined(HELPER_EXECUTABLE)

  // Slice off the last part of the main executable path, and append the
  // version framework information.
  const char* parent_dir = dirname(exec_path);
  if (!parent_dir) {
    AperitifFatalError("dirname %s: %s.", exec_path, strerror(errno));
  }

  char framework_path[PATH_MAX];
  rv = snprintf(framework_path, sizeof(framework_path), "%s/%s", parent_dir,
                rel_path);
  if (rv < 0 || (size_t)rv >= sizeof(framework_path)) {
    AperitifFatalError("snprintf: %d.", rv);
  }

  void* library = dlopen(framework_path, RTLD_LAZY | RTLD_LOCAL | RTLD_FIRST);
  if (!library) {
    AperitifFatalError("dlopen %s: %s.", framework_path, dlerror());
  }

  const ChromeMainPtr chrome_main = dlsym(library, "ChromeMain");
  if (!chrome_main) {
    AperitifFatalError("dlsym ChromeMain: %s.", dlerror());
  }
  rv = chrome_main(argc, argv);

  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(rv);
}
