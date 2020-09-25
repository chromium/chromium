// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/immediate_crash.h"

#include <stdint.h>

#include "base/base_paths.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Compile test.
int ALLOW_UNUSED_TYPE TestImmediateCrashTreatedAsNoReturn() {
  IMMEDIATE_CRASH();
}

#if defined(ARCH_CPU_X86_FAMILY)
// This is tricksy and false, since x86 instructions are not all one byte long,
// but there is no better alternative short of implementing an x86 instruction
// decoder.
using Instruction = uint8_t;

// https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-1-2a-2b-2c-2d-3a-3b-3c-3d-and-4
// Look for RET opcode (0xc3). Note that 0xC3 is a substring of several
// other opcodes (VMRESUME, MOVNTI), and can also be encoded as part of an
// argument to another opcode. None of these other cases are expected to be
// present, so a simple byte scan should be Good Enough™.
constexpr Instruction kRet = 0xc3;
// INT3 ; UD2
constexpr Instruction kRequiredBody[] = {0xcc, 0x0f, 0x0b};
constexpr Instruction kOptionalFooter[] = {};

#elif defined(ARCH_CPU_ARMEL)
using Instruction = uint16_t;

// T32 opcode reference: https://developer.arm.com/docs/ddi0487/latest
// Actually BX LR, canonical encoding:
constexpr Instruction kRet = 0x4770;
// BKPT #0; UDF #0
constexpr Instruction kRequiredBody[] = {0xbe00, 0xde00};
constexpr Instruction kOptionalFooter[] = {};

#elif defined(ARCH_CPU_ARM64)
using Instruction = uint32_t;

// A64 opcode reference: https://developer.arm.com/docs/ddi0487/latest
// Use an enum here rather than separate constexpr vars because otherwise some
// of the vars will end up unused on each platform, upsetting
// -Wunused-const-variable.
enum {
  // There are multiple valid encodings of return (which is really a special
  // form of branch). This is the one clang seems to use:
  kRet = 0xd65f03c0,
  kBrk0 = 0xd4200000,
  kBrk1 = 0xd4200020,
  kBrkF000 = 0xd43e0000,
  kHlt0 = 0xd4400000,
};

#if defined(OS_WIN)

constexpr Instruction kRequiredBody[] = {kBrkF000, kBrk1};
constexpr Instruction kOptionalFooter[] = {};

#elif defined(OS_MAC)

constexpr Instruction kRequiredBody[] = {kBrk0, kHlt0};
// Some clangs emit a BRK #1 for __builtin_unreachable(), but some do not, so
// it is allowed but not required to occur.
constexpr Instruction kOptionalFooter[] = {kBrk1};

#else

constexpr Instruction kRequiredBody[] = {kBrk0, kHlt0};
constexpr Instruction kOptionalFooter[] = {};

#endif

#endif

// This function loads a shared library that defines two functions,
// TestFunction1 and TestFunction2. It then returns the bytes of the body of
// whichever of those functions happens to come first in the library.
void GetTestFunctionInstructions(std::vector<Instruction>* body) {
  FilePath helper_library_path;
#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  // On Android M, DIR_EXE == /system/bin when running base_unittests.
  // On Fuchsia, NativeLibrary understands the native convention that libraries
  // are not colocated with the binary.
  ASSERT_TRUE(PathService::Get(DIR_EXE, &helper_library_path));
#endif
  helper_library_path = helper_library_path.AppendASCII(
      GetNativeLibraryName("immediate_crash_test_helper"));
#if defined(OS_ANDROID) && defined(COMPONENT_BUILD)
  helper_library_path = helper_library_path.ReplaceExtension(".cr.so");
#endif
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

base::Optional<std::vector<Instruction>> ExpectImmediateCrashInvocation(
    std::vector<Instruction> instructions) {
  auto iter = instructions.begin();
  for (const auto inst : kRequiredBody) {
    if (iter == instructions.end())
      return base::nullopt;
    EXPECT_EQ(inst, *iter);
    iter++;
  }
  return base::make_optional(
      std::vector<Instruction>(iter, instructions.end()));
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

}  // namespace

// Checks that the IMMEDIATE_CRASH() macro produces specific instructions; see
// comments in immediate_crash.h for the requirements.
TEST(ImmediateCrashTest, ExpectedOpcodeSequence) {
  std::vector<Instruction> body;
  ASSERT_NO_FATAL_FAILURE(GetTestFunctionInstructions(&body));
  SCOPED_TRACE(HexEncode(body.data(), body.size() * sizeof(Instruction)));

  auto it = std::find(body.begin(), body.end(), kRet);
  ASSERT_NE(body.end(), it) << "Failed to find return opcode";
  it++;

  body = std::vector<Instruction>(it, body.end());
  auto result = ExpectImmediateCrashInvocation(body);
  result = MaybeSkipOptionalFooter(result.value());
  result = ExpectImmediateCrashInvocation(result.value());
  ASSERT_TRUE(result);
}

}  // namespace base
