// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/rosetta.h"

#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <mach/machine.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "base/files/file_path.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

namespace base {
namespace mac {

#if defined(ARCH_CPU_X86_64)

// https://developer.apple.com/documentation/apple_silicon/about_the_rosetta_translation_environment#3616845
bool ProcessIsTranslated() {
  int ret = 0;
  size_t size = sizeof(ret);
  if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) == -1)
    return false;
  return ret;
}

#endif  // ARCH_CPU_X86_64

#if defined(ARCH_CPU_ARM64)

bool IsRosettaInstalled() {
  // Chromium currently requires the 10.15 SDK, but code compiled for Arm must
  // be compiled against at least the 11.0 SDK and will run on at least macOS
  // 11.0, so this is safe. __builtin_available doesn't work for 11.0 yet;
  // https://crbug.com/1115294
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
  return CFBundleIsArchitectureLoadable(CPU_TYPE_X86_64);
#pragma clang diagnostic pop
}

#endif  // ARCH_CPU_ARM64

bool RequestRosettaAheadOfTimeTranslation(
    const std::vector<FilePath>& binaries) {
#if defined(ARCH_CPU_X86_64)
  if (!ProcessIsTranslated())
    return false;
#endif  // ARCH_CPU_X86_64

  // It's unclear if this is a BOOL or Boolean or bool. It's returning 0 or 1
  // in w0, so just grab the result as an int and convert here.
  // int oah_translate_binaries(const char*[] paths, int npaths)
  using oah_translate_binaries_t = int (*)(const char*[], int);
  static auto oah_translate_binaries = []() {
    void* liboah = dlopen("/usr/lib/liboah.dylib", RTLD_LAZY | RTLD_LOCAL);
    if (!liboah)
      return static_cast<oah_translate_binaries_t>(nullptr);
    return reinterpret_cast<oah_translate_binaries_t>(
        dlsym(liboah, "oah_translate_binaries"));
  }();
  if (!oah_translate_binaries)
    return false;

  std::vector<std::string> paths;
  std::vector<const char*> path_strs;
  paths.reserve(binaries.size());
  path_strs.reserve(paths.size());
  for (const auto& binary : binaries) {
    paths.push_back(FilePath::GetHFSDecomposedForm(binary.value()));
    path_strs.push_back(paths.back().c_str());
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return oah_translate_binaries(path_strs.data(), path_strs.size());
}

}  // namespace mac
}  // namespace base
