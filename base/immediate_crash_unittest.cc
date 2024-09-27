// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/immediate_crash.h"

#include <stdint.h>

#include <optional>

#include "base/base_paths.h"
#include "base/clang_profiling_buildflags.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// If ImmediateCrash() is not treated as noreturn by the compiler, the compiler
// will complain that not all paths through this function return a value.
[[maybe_unused]] int TestImmediateCrashTreatedAsNoReturn() {
  ImmediateCrash();
}

#if defined(ARCH_CPU_X86_FAMILY)
// This is tricksy and false, since x86 instructions are not all one byte long,
// but there is no better alternative short of implementing an x86 instruction
// decoder.
using Instruction = uint8_t;

#if defined(OFFICIAL_BUILD)
// https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-1-2a-2b-2c-2d-3a-3b-3c-3d-and-4
// Look for RET opcode (0xc3). Note that 0xC3 is a substring of several
// other opcodes (VMRESUME, MOVNTI), and can also be encoded as part of an
// argument to another opcode. None of these other cases are expected to be
// present, so a simple byte scan should be Good Enough™.
constexpr Instruction kRet = 0xc3;
// INT3 ; UD2

constexpr Instruction kRequiredBody[] = {0xcc, 0x0f, 0x0b};
constexpr Instruction kOptionalFooter[] = {};
#endif  // defined(OFFICIAL_BUILD)

#elif defined(ARCH_CPU_ARMEL)
using Instruction = uint16_t;

#if defined(OFFICIAL_BUILD)
// T32 opcode reference: https://developer.arm.com/docs/ddi0487/latest
// Actually BX LR, canonical encoding:
constexpr Instruction kRet = 0x4770;

// BKPT #0; UDF #0
constexpr Instruction kRequiredBody[] = {0xbe00, 0xde00};
constexpr Instruction kOptionalFooter[] = {};
#endif  // defined(OFFICIAL_BUILD)

#elif defined(ARCH_CPU_ARM64)
using Instruction = uint32_t;

#if defined(OFFICIAL_BUILD)
// A64 opcode reference: https://developer.arm.com/docs/ddi0487/latest
// Use an enum here rather than separate constexpr vars because otherwise some
// of the vars will end up unused on each platform, upsetting
// -Wunused-const-variable.
enum : Instruction {
  // There are multiple valid encodings of return (which is really a special
  // form of branch). This is the one clang seems to use:
  kRet = 0xd65f03c0,
  kBrk0 = 0xd4200000,
  kBrk1 = 0xd4200020,
  kBrkF000 = 0xd43e0000,
  kHlt0 = 0xd4400000,
};

#if BUILDFLAG(IS_WIN)

constexpr Instruction kRequiredBody[] = {kBrkF000, kBrk1};
constexpr Instruction kOptionalFooter[] = {};

#elif BUILDFLAG(IS_MAC)

constexpr Instruction kRequiredBody[] = {kBrk0, kHlt0};
// Some clangs emit a BRK #1 for __builtin_unreachable(), but some do not, so
// it is allowed but not required to occur.
constexpr Instruction kOptionalFooter[] = {kBrk1};

#else

constexpr Instruction kRequiredBody[] = {kBrk0, kHlt0};
constexpr Instruction kOptionalFooter[] = {};

#endif

#endif  // defined(OFFICIAL_BUILD)

#endif

// This function loads a shared library that defines two functions,
// TestFunction1 and TestFunction2. It then returns the bytes of the body of
// whichever of those functions happens to come first in the library.
void GetTestFunctionInstructions(std::vector<Instruction>* body) {
  FilePath helper_library_path;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  // On Android M, DIR_EXE == /system/bin when running base_unittests.
  // On Fuchsia, NativeLibrary understands the native convention that libraries
  // are not colocated with the binary.
  ASSERT_TRUE(PathService::Get(DIR_EXE, &helper_library_path));
#endif
  helper_library_path = helper_library_path.AppendASCII(
      GetNativeLibraryName("immediate_crash_test_helper"));
  ScopedNativeLibrary helper_library(helper_library_path);
  ASSERT_TRUE(helper_library.is_valid())
      << "shared library load failed: "
      << helper_library.GetError()->ToString();

  void* a = helper_library.GetFunctionPointer("TestFunction1");
  ASSERT_TRUE(a);
  void* b = helper_library.GetFunctionPointer("TestFunction2");
  ASSERT_TRUE(b);

#if defined(ARCH_CPU_ARMEL)
  // Routines loaded from a shared library will have the LSB in the pointer set
  // if encoded as T32 instructions. The rest of this test assumes T32.
  ASSERT_TRUE(reinterpret_cast<uintptr_t>(a) & 0x1)
      << "Expected T32 opcodes but found A32 opcodes instead.";
  ASSERT_TRUE(reinterpret_cast<uintptr_t>(b) & 0x1)
      << "Expected T32 opcodes but found A32 opcodes instead.";

  // Mask off the lowest bit.
  a = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(a) & ~uintptr_t{0x1});
  b = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(b) & ~uintptr_t{0x1});
#endif

  // There are two identical test functions starting at a and b, which may
  // occur in the library in either order. Grab whichever one comes first,
  // and use the address of the other to figure out where it ends.
  const Instruction* const start = static_cast<Instruction*>(std::min(a, b));
  const Instruction* const end = static_cast<Instruction*>(std::max(a, b));

  for (const Instruction& instruction : make_span(start, end))
    body->push_back(instruction);
}

#if defined(OFFICIAL_BUILD)

std::optional<std::vector<Instruction>> ExpectImmediateCrashInvocation(
    std::vector<Instruction> instructions) {
  auto iter = instructions.begin();
  for (const auto inst : kRequiredBody) {
    if (iter == instructions.end())
      return std::nullopt;
    EXPECT_EQ(inst, *iter);
    iter++;
  }
  return std::make_optional(std::vector<Instruction>(iter, instructions.end()));
}

std::vector<Instruction> MaybeSkipOptionalFooter(
    std::vector<Instruction> instructions) {
  auto iter = instructions.begin();
  for (const auto inst : kOptionalFooter) {
    if (iter == instructions.end() || *iter != inst)
      break;
    iter++;
  }
  return std::vector<Instruction>(iter, instructions.end());
}

#if BUILDFLAG(USE_CLANG_COVERAGE) || BUILDFLAG(CLANG_PROFILING)
bool MatchPrefix(const std::vector<Instruction>& haystack,
                 const base::span<const Instruction>& needle) {
  for (size_t i = 0; i < needle.size(); i++) {
    if (i >= haystack.size() || needle[i] != haystack[i])
      return false;
  }
  return true;
}

std::vector<Instruction> DropUntilMatch(
    std::vector<Instruction> haystack,
    const base::span<const Instruction>& needle) {
  while (!haystack.empty() && !MatchPrefix(haystack, needle))
    haystack.erase(haystack.begin());
  return haystack;
}

#endif  // USE_CLANG_COVERAGE || BUILDFLAG(CLANG_PROFILING)

std::vector<Instruction> MaybeSkipCoverageHook(
    std::vector<Instruction> instructions) {
#if BUILDFLAG(USE_CLANG_COVERAGE) || BUILDFLAG(CLANG_PROFILING)
  // Warning: it is not illegal for the entirety of the expected crash sequence
  // to appear as a subsequence of the coverage hook code. If that happens, this
  // code will falsely exit early, having not found the real expected crash
  // sequence, so this may not adequately ensure that the immediate crash
  // sequence is present. We do check when not under coverage, at least.
  return DropUntilMatch(instructions, base::make_span(kRequiredBody));
#else
  return instructions;
#endif  // USE_CLANG_COVERAGE || BUILDFLAG(CLANG_PROFILING)
}

#endif  // defined(OFFICIAL_BUILD)

}  // namespace

// Attempts to verify the actual instructions emitted by ImmediateCrash().
// While the test results are highly implementation-specific, this allows macro
// changes (e.g. CLs like https://crrev.com/671123) to be verified using the
// trybots/waterfall, without having to build and disassemble Chrome on
// multiple platforms. This makes it easier to evaluate changes to
// ImmediateCrash() against its requirements (e.g. size of emitted sequence,
// whether or not multiple ImmediateCrash sequences can be folded together, et
// cetera). Please see immediate_crash.h for more details about the
// requirements.
//
// Note that C++ provides no way to get the size of a function. Instead, the
// test relies on a shared library which defines only two functions and assumes
// the two functions will be laid out contiguously as a heuristic for finding
// the size of the function.
TEST(ImmediateCrashTest, ExpectedOpcodeSequence) {
  std::vector<Instruction> body;
  ASSERT_NO_FATAL_FAILURE(GetTestFunctionInstructions(&body));
  SCOPED_TRACE(HexEncode(body.data(), body.size() * sizeof(Instruction)));

  // In non-official builds, we std::abort instead, so the result will be
  // false - but let's still go through the motions above so we spot any
  // problems in this _test code_ in as many build permutations as possible.
#if defined(OFFICIAL_BUILD)
  auto it = ranges::find(body, kRet);
  ASSERT_NE(body.end(), it) << "Failed to find return opcode";
  it++;

  body = std::vector<Instruction>(it, body.end());
  std::optional<std::vector<Instruction>> result = MaybeSkipCoverageHook(body);
  result = ExpectImmediateCrashInvocation(result.value());
  result = MaybeSkipOptionalFooter(result.value());
  result = MaybeSkipCoverageHook(result.value());
  result = ExpectImmediateCrashInvocation(result.value());
  ASSERT_TRUE(result);
#endif  // defined(OFFICIAL_BUILD)
}

}  // namespace base
