// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entry point for all Mac Chromium processes, including the outer app
// bundle (browser) and helper app (renderer, plugin, and friends).

#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>

#include "base/allocator/early_zone_registration_apple.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/chrome_version.h"

#if defined(HELPER_EXECUTABLE)
#include "sandbox/mac/seatbelt_exec.h"  // nogncheck
#endif

extern "C" {
// abort_report_np() records the message in a special section that both the
// system CrashReporter and Crashpad collect in crash reports. Using a Crashpad
// Annotation would be preferable, but this executable cannot depend on
// Crashpad directly.
void abort_report_np(const char* fmt, ...);
}

namespace {

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
// at offset 288kB (since 123.0.6312.10). The signed x86_64 slice size has grown
// from 249072 in 122.0.6261.143 to 269712 in 123.0.6312.10 (including 56kB of
// current padding). Future versions need to be padded to be in the range
// (262144, 278528] so that the arm64 slice that follows it begins at offset
// 288kB. To allow for the possibility of small-scale (up to +/-8kB) size
// changes, the target size for the padded x86_64 slice is 270336 bytes.

// As of this writing 125.0.6378.0's x86_64 slice has shrunk back down to
// 249312. To make up the 78368-byte difference, 76kB (77824 bytes) of padding
// is added to the x86_64 slice to ensure that its size is stable, causing the
// arm64 slice to land where it needs to be when universalized. This padding
// needs to be added to the thin form of the x86_64 image before being fed to
// universalizer.py. Why 77824 bytes and not 78368? To keep it an even multiple
// of linker pages (not machine pages: linker pages are 4kB for lld targeting
// x86_64 and 16kB for ld64 targeting x86_64, but Chrome uses lld). In any case,
// I'll make up some of the 544-byte difference with one more weird trick below.
//
// There are several terrible ways to insert this padding into the x86_64 image.
// Best would be something that considers the size of the x86_64 image without
// padding, and inserts the precise amount required. It may be possible to do
// this after linking, but the options that have been attempted so far were not
// successful. So this quick and very dirty 56kB buffer is added to increase the
// size of __TEXT,__const in a way that no tool could possibly see as suspicious
// after link time. The variable is marked with the "used" attribute to prevent
// the compiler from issuing warnings about the referenced variable, to prevent
// the compiler from removing it under optimization, and to set the
// S_ATTR_NO_DEAD_STRIP section attribute to prevent the linker from removing it
// under -dead_strip. Note that the standardized [[maybe_unused]] attribute only
// suppresses the warning, but does not prevent the compiler or linker from
// removing it.
//
// The introduction of this fixed 76kB of padding causes the unsigned linker
// output to grow by 76kB precisely, but the signed output will grow by slightly
// more. This is because the code signature's code directory contains SHA-1 and
// SHA-256 hashes of each 4kB code signing page (note, not machine pages or
// linker pages) in the image, adding 20 and 32 bytes each (macOS 12.0.1
// https://github.com/apple-oss-distributions/Security/blob/main/OSX/libsecurity_codesigning/lib/signer.cpp#L298
// Security::CodeSigning::SecCodeSigner::Signer::prepare). For the 76kB
// addition, the code signature grows by (76 / 4) * (20 + 32) = 998 bytes, thus
// the total size of the linker output grows by 76kB + 998 = 78822 bytes. It is
// not possible to control this any more granularly: if the buffer were sized at
// 76kB - 998 = 76826 bytes, it would either cause no change in the space
// allocated to the __TEXT segment (due to padding for alignment) or would cause
// the segment to shrink by a linker page (note, not a code signing or machine
// page) which would which would cause the linker output to shrink by the same
// amount and would be absolutely undesirable. Luckily, the net growth of 78822
// bytes is almost at the target of 78368. In any event, having the signed
// x86_64 slice sized at 269792 bytes instead of 270336 should not be a problem.
// So long as the size is in the proper 16kB range, the 16kB alignment for the
// arm64 slice that follows it in the fat file will cause it to appear at the
// desired 288kB.
//
// If the main executable has a significant change in size, this will need to be
// revised. Hopefully a more elegant solution will become apparent before that's
// required.
#if !defined(DCHECK_ALWAYS_ON)
__attribute__((used)) const char kGrossPaddingForCrbug1300598[84 * 1024] = {};
#else
// DCHECK builds are larger and therefore require less padding. See
// https://crbug.com/1394196 for the calculations, and
// https://crbug.com/357698332 for further follow-up.
__attribute__((used)) const char kGrossPaddingForCrbug1300598[44 * 1024] = {};
#endif  // !defined(DCHECK_ALWAYS_ON)
#endif

[[noreturn]] void FatalError(const char* format, ...) {
  va_list valist;
  va_start(valist, format);
  char message[4096];
  if (vsnprintf(message, sizeof(message), format, valist) >= 0) {
    fputs(message, stderr);
    abort_report_np("%s", message);
  }
  va_end(valist);
  abort();
}

}  // namespace

__attribute__((visibility("default"))) int main(int argc, char* argv[]) {
  partition_alloc::EarlyMallocZoneRegistration();

  uint32_t exec_path_size = 0;
  int rv = _NSGetExecutablePath(NULL, &exec_path_size);
  if (rv != -1) {
    FatalError("_NSGetExecutablePath: get length failed.");
  }

  std::unique_ptr<char[]> exec_path(new char[exec_path_size]);
  rv = _NSGetExecutablePath(exec_path.get(), &exec_path_size);
  if (rv != 0) {
    FatalError("_NSGetExecutablePath: get path failed.");
  }

#if defined(HELPER_EXECUTABLE)
  sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
      sandbox::SeatbeltExecServer::CreateFromArguments(exec_path.get(), argc,
                                                       argv);
  if (seatbelt.sandbox_required) {
    if (!seatbelt.server) {
      FatalError("Failed to create seatbelt sandbox server.");
    }
    if (!seatbelt.server->InitializeSandbox()) {
      FatalError("Failed to initialize sandbox.");
    }
  }

  // The helper lives within the versioned framework directory, so simply
  // go up to find the main dylib.
  const char rel_path[] = "../../../../" PRODUCT_FULLNAME_STRING " Framework";
#else
  const char rel_path[] = "../Frameworks/" PRODUCT_FULLNAME_STRING
                          " Framework.framework/Versions/" CHROME_VERSION_STRING
                          "/" PRODUCT_FULLNAME_STRING " Framework";
#endif  // defined(HELPER_EXECUTABLE)

  // Slice off the last part of the main executable path, and append the
  // version framework information.
  const char* parent_dir = dirname(exec_path.get());
  if (!parent_dir) {
    FatalError("dirname %s: %s.", exec_path.get(), strerror(errno));
  }

  const size_t parent_dir_len = strlen(parent_dir);
  const size_t rel_path_len = strlen(rel_path);
  // 2 accounts for a trailing NUL byte and the '/' in the middle of the paths.
  const size_t framework_path_size = parent_dir_len + rel_path_len + 2;
  std::unique_ptr<char[]> framework_path(new char[framework_path_size]);
  snprintf(framework_path.get(), framework_path_size, "%s/%s", parent_dir,
           rel_path);

  void* library =
      dlopen(framework_path.get(), RTLD_LAZY | RTLD_LOCAL | RTLD_FIRST);
  if (!library) {
    FatalError("dlopen %s: %s.", framework_path.get(), dlerror());
  }

  const ChromeMainPtr chrome_main =
      reinterpret_cast<ChromeMainPtr>(dlsym(library, "ChromeMain"));
  if (!chrome_main) {
    FatalError("dlsym ChromeMain: %s.", dlerror());
  }
  rv = chrome_main(argc, argv);

  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(rv);
}
