// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entry point for all Mac Chromium processes, including the outer app
// bundle (browser) and helper app (renderer and friends).

#include <stdlib.h>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "content/public/app/aperitif_mac.h"

int ChromeMain(int, char**);

__attribute__((constructor)) static void CheckAperitif() {
  AperitifCheckInitialized();
}

__attribute__((visibility("default"))) int main(int argc, char* argv[]) {
  int rv = ChromeMain(argc, argv);

  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(rv);
}

#if !defined(HELPER_EXECUTABLE) && defined(OFFICIAL_BUILD) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(ARCH_CPU_X86_64)
// This is for https://crbug.com/40216333, and more generally,
// https://crbug.com/40215211 (and all of the associated bugs). It's horrible!
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
// at offset 288kB (since 123.0.6312.10).
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
// The arm64 slice will be 16kB-aligned, so as long as the signed x86_64 slice
// ends anywhere in the offset range (272kB, 288kB], the desired alignment will
// be preserved. The x86_64 slice begins at offset 16kB (the fat header precedes
// it, and it is also 16kB-aligned), so the signed x86_64 slice’s size must be
// in the range (256kB, 272kB] in order for the slice’s end to be in the
// required range of offsets. To allow for small amounts of growth and
// shrinkage, the signed x86_64 slice’s size should target the middle of this
// range, or 264kB.
//
// At build time, the signed slice’s size is not known. Although subject to
// change, recent (2025-05, 138.0.7160) code signatures for official builds of
// the correct size introduce 22656 extra bytes beyond the size of the unsigned
// slice. With this signature length in mind, the size target for the unsigned
// x86_64 slice is 264kB - 22656 = 247680.
//
// With an unpadded unsigned size of 27024, (247680 - 27024) = 220656 bytes of
// padding are desirable. The padding can only be introduced with 4kB precision,
// so 216kB of padding is introduced.
//
// If the main executable has a significant change in size, this will need to be
// revised.
//
// If you’re here because of an InvalidAppGeometryException (checked at code
// signing time), recalculate the required padding for the x86_64 slice: take
// the reported signed x86_64 slice’s size reported by lipo -detailed_info and
// subtract 264k (270336) from it. If positive, remove padding in 4kB
// increments, and if negative, add padding in 4kB increments. The objective is
// to arrive at a signed x86_64 slice whose size is as close to 264kB as
// possible.
//
// (Each 4kB page added or removed here will result in slightly more than 4kB
// added or removed from the signed slice: it’s actually 4kB plus 32 bytes, 4128
// bytes total, accounting for both the padding and the additional SHA-256 hash
// incorporated into the code signature. The difference is <1% and can be
// ignored in most cases.)
//
// TODO(crbug.com/40794783): This padding size was borrowed from the
// non-Apéritif chrome_exe_main_mac.cc and will need to be recomputed for the
// Apéritif chrome_exe_main_mac.c once Apéritif is enabled for the main
// browser executable.
__attribute__((used)) const char kGrossPaddingForCrbug1300598[216 * 1024] = {};
#endif
