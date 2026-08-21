// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/llvm_profile_util.h"

#include <string>
#include <string_view>

#include "build/build_config.h"
#include "build/config/compiler/compiler_buildflags.h"

#if BUILDFLAG(CLANG_PGO_PROFILING)
extern "C" {
void __llvm_profile_set_filename(const char* Name);
const char* __llvm_profile_get_filename(void);
}
#endif

namespace {

std::string_view GetProfilePrefix(ProfileProcessType process_type) {
  switch (process_type) {
    case ProfileProcessType::kRenderer:
      return "renderer";
  }
}

}  // namespace

std::string GetLLVMProfileFilename(std::string_view current,
                                   ProfileProcessType process_type) {
  if (current.empty()) {
    return std::string();
  }

  // Find the start of the filename. We deliberately manipulate raw byte strings
  // rather than using base::FilePath:
  // * LLVM's compiler-rt runtime takes a `const char*` and passes it directly
  //   to `fopen()`. Preserving the parent path's exact raw bytes prevents any
  //   encoding corruption that could occur when round-tripping through UTF-16.
  // * Minimal bootstrap targets (such as Mac helper executables) can use this
  //   utility without taking a dependency on //base.
#if BUILDFLAG(IS_WIN)
  constexpr std::string_view kPathSeparators = "/\\";
#else
  constexpr std::string_view kPathSeparators = "/";
#endif
  size_t filename_start = current.find_last_of(kPathSeparators);
  filename_start =
      (filename_start == std::string_view::npos) ? 0 : filename_start + 1;

  // Only rewrite the filename if it starts with "default-" (the pattern set by
  // `tools/pgo/generate_profile.py`). If a custom filename is specified (e.g.
  // during manual profiling runs or tests), leave it untouched rather than
  // failing or crashing.
  std::string_view filename = current.substr(filename_start);
  constexpr std::string_view kDefaultPrefix = "default-";
  if (!filename.starts_with(kDefaultPrefix)) {
    return std::string();
  }

  std::string_view prefix = GetProfilePrefix(process_type);
  return std::string(current.substr(0, filename_start)) + std::string(prefix) +
         "-" + std::string(filename.substr(kDefaultPrefix.size()));
}

void SetLLVMProfileProcessType(ProfileProcessType process_type) {
#if BUILDFLAG(CLANG_PGO_PROFILING)
  const char* current = __llvm_profile_get_filename();
  if (!current) {
    return;
  }

  std::string new_path = GetLLVMProfileFilename(current, process_type);
  if (!new_path.empty()) {
    __llvm_profile_set_filename(new_path.c_str());
  }
#endif
}
