// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/win/com_init_check_hook.h"

#include <objbase.h>

#include <windows.h>

#include <stdint.h>
#include <string.h>

#include <ostream>
#include <string>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/win/com_init_util.h"
#include "base/win/patch_util.h"

namespace base {
namespace win {

#if defined(COM_INIT_CHECK_HOOK_ENABLED)

namespace {

// Hotpatchable Microsoft x86 32-bit functions take one of two forms:
// Newer format:
// RelAddr  Binary     Instruction                 Remarks
//      -5  cc         int 3
//      -4  cc         int 3
//      -3  cc         int 3
//      -2  cc         int 3
//      -1  cc         int 3
//       0  8bff       mov edi,edi                 Actual entry point and no-op.
//       2  ...                                    Actual body.
//
// Older format:
// RelAddr  Binary     Instruction                 Remarks
//      -5  90         nop
//      -4  90         nop
//      -3  90         nop
//      -2  90         nop
//      -1  90         nop
//       0  8bff       mov edi,edi                 Actual entry point and no-op.
//       2  ...                                    Actual body.
//
// The "int 3" or nop sled as well as entry point no-op are critical, as they
// are just enough to patch in a short backwards jump to -5 (2 bytes) then that
// can do a relative 32-bit jump about 2GB before or after the current address.
//
// To perform a hotpatch, we need to figure out where we want to go and where
// we are now as the final jump is relative. Let's say we want to jump to
// 0x12345678. Relative jumps are calculated from eip, which for our jump is the
// next instruction address. For the example above, that means we start at a 0
// base address.
//
// Our patch will then look as follows:
// RelAddr  Binary     Instruction                 Remarks
//      -5  e978563412 jmp 0x12345678-(-0x5+0x5)   Note little-endian format.
//       0  ebf9       jmp -0x5-(0x0+0x2)          Goes to RelAddr -0x5.
//       2  ...                                    Actual body.
// Note: The jmp instructions above are structured as
//       Address(Destination)-(Address(jmp Instruction)+sizeof(jmp Instruction))

// The struct below is provided for convenience and must be packed together byte
// by byte with no word alignment padding. This comes at a very small
// performance cost because now there are shifts handling the fields, but
// it improves readability.
#pragma pack(push, 1)
struct StructuredHotpatch {
  unsigned char jmp_32_relative = 0xe9;  // jmp relative 32-bit.
  int32_t relative_address = 0;          // 32-bit signed operand.
  unsigned char jmp_8_relative = 0xeb;   // jmp relative 8-bit.
  unsigned char back_address = 0xf9;     // Operand of -7.
};
#pragma pack(pop)

static_assert(sizeof(StructuredHotpatch) == 7,
              "Needs to be exactly 7 bytes for the hotpatch to work.");

// nop Function Padding with "mov edi,edi"
const unsigned char g_hotpatch_placeholder_nop[] = {0x90, 0x90, 0x90, 0x90,
                                                    0x90, 0x8b, 0xff};

// int 3 Function Padding with "mov edi,edi"
const unsigned char g_hotpatch_placeholder_int3[] = {0xcc, 0xcc, 0xcc, 0xcc,
                                                     0xcc, 0x8b, 0xff};

// http://crbug.com/1312659: Unusable apphelp placeholder missing one byte.
const unsigned char g_hotpatch_placeholder_apphelp[] = {0x00, 0xcc, 0xcc, 0xcc,
                                                        0xcc, 0x8b, 0xff};

class HookManager {
 public:
  static HookManager* GetInstance() {
    static auto* hook_manager = new HookManager();
    return hook_manager;
  }

  HookManager(const HookManager&) = delete;
  HookManager& operator=(const HookManager&) = delete;

  void RegisterHook() {
    AutoLock auto_lock(lock_);
    ++init_count_;
    if (disabled_)
      return;
    if (init_count_ == 1)
      WriteHook();
  }

  void UnregisterHook() {
    AutoLock auto_lock(lock_);
    DCHECK_NE(0U, init_count_);
    --init_count_;
    if (disabled_)
      return;
    if (init_count_ == 0)
      RevertHook();
  }

  void DisableCOMChecksForProcess() {
    AutoLock auto_lock(lock_);
    if (disabled_)
      return;
    disabled_ = true;
    if (init_count_ > 0)
      RevertHook();
  }

 private:
  enum class HotpatchPlaceholderFormat {
    // The hotpatch placeholder is currently unknown
    UNKNOWN,
    // The hotpatch placeholder used int 3's in the sled.
    INT3,
    // The hotpatch placeholder used nop's in the sled.
    NOP,
    // The hotpatch placeholder is an unusable apphelp shim.
    APPHELP_SHIM,
    // This function has already been patched by a different component.
    EXTERNALLY_PATCHED,
  };

  HookManager() = default;
  ~HookManager() = default;

  void WriteHook() {
    lock_.AssertAcquired();
    DCHECK(!ole32_library_);
    ole32_library_ = ::LoadLibrary(L"ole32.dll");

    if (!ole32_library_)
      return;

    // See banner comment above why this subtracts 5 bytes.
    co_create_instance_padded_address_ =
        reinterpret_cast<uint32_t>(
            GetProcAddress(ole32_library_, "CoCreateInstance")) -
        5;

    // See banner comment above why this adds 7 bytes.
    original_co_create_instance_body_function_ =
        reinterpret_cast<decltype(original_co_create_instance_body_function_)>(
            co_create_instance_padded_address_ + 7);

    uint32_t dchecked_co_create_instance_address =
        reinterpret_cast<uint32_t>(&HookManager::DCheckedCoCreateInstance);
    uint32_t jmp_offset_base_address = co_create_instance_padded_address_ + 5;
    structured_hotpatch_.relative_address = static_cast<int32_t>(
        dchecked_co_create_instance_address - jmp_offset_base_address);

    HotpatchPlaceholderFormat format = GetHotpatchPlaceholderFormat(
        reinterpret_cast<const void*>(co_create_instance_padded_address_));
    if (format == HotpatchPlaceholderFormat::UNKNOWN) {
      NOTREACHED() << "Unrecognized hotpatch function format: "
                   << FirstSevenBytesToString(
                          co_create_instance_padded_address_);
    } else if (format == HotpatchPlaceholderFormat::EXTERNALLY_PATCHED) {
      NOTREACHED() << "CoCreateInstance appears to be previously patched. <"
                   << FirstSevenBytesToString(
                          co_create_instance_padded_address_)
                   << "> Attempted to write <"
                   << FirstSevenBytesToString(
                          reinterpret_cast<uint32_t>(&structured_hotpatch_))
                   << ">";
    } else if (format == HotpatchPlaceholderFormat::APPHELP_SHIM) {
      // The apphelp shim placeholder does not allocate enough bytes for a
      // trampolined jump. In this case, we skip patching.
      hotpatch_placeholder_format_ = format;
      return;
    }

    DCHECK_EQ(hotpatch_placeholder_format_, HotpatchPlaceholderFormat::UNKNOWN);
    DWORD patch_result = internal::ModifyCode(
        reinterpret_cast<void*>(co_create_instance_padded_address_),
        reinterpret_cast<void*>(&structured_hotpatch_),
        sizeof(structured_hotpatch_));
    if (patch_result == NO_ERROR)
      hotpatch_placeholder_format_ = format;
  }

  void RevertHook() {
    lock_.AssertAcquired();

    DWORD revert_result = NO_ERROR;
    switch (hotpatch_placeholder_format_) {
      case HotpatchPlaceholderFormat::INT3:
        if (WasHotpatchChanged())
          return;
        revert_result = internal::ModifyCode(
            reinterpret_cast<void*>(co_create_instance_padded_address_),
            reinterpret_cast<const void*>(&g_hotpatch_placeholder_int3),
            sizeof(g_hotpatch_placeholder_int3));
        break;
      case HotpatchPlaceholderFormat::NOP:
        if (WasHotpatchChanged())
          return;
        revert_result = internal::ModifyCode(
            reinterpret_cast<void*>(co_create_instance_padded_address_),
            reinterpret_cast<const void*>(&g_hotpatch_placeholder_nop),
            sizeof(g_hotpatch_placeholder_nop));
        break;
      case HotpatchPlaceholderFormat::EXTERNALLY_PATCHED:
      case HotpatchPlaceholderFormat::APPHELP_SHIM:
      case HotpatchPlaceholderFormat::UNKNOWN:
        break;
    }
    DCHECK_EQ(revert_result, static_cast<DWORD>(NO_ERROR))
        << "Failed to revert CoCreateInstance hot-patch";

    hotpatch_placeholder_format_ = HotpatchPlaceholderFormat::UNKNOWN;

    if (ole32_library_) {
      ::FreeLibrary(ole32_library_);
      ole32_library_ = nullptr;
    }

    co_create_instance_padded_address_ = 0;
    original_co_create_instance_body_function_ = nullptr;
  }

  HotpatchPlaceholderFormat GetHotpatchPlaceholderFormat(const void* address) {
    if (::memcmp(reinterpret_cast<void*>(co_create_instance_padded_address_),
                 reinterpret_cast<const void*>(&g_hotpatch_placeholder_int3),
                 sizeof(g_hotpatch_placeholder_int3)) == 0) {
      return HotpatchPlaceholderFormat::INT3;
    }

    if (::memcmp(reinterpret_cast<void*>(co_create_instance_padded_address_),
                 reinterpret_cast<const void*>(&g_hotpatch_placeholder_nop),
                 sizeof(g_hotpatch_placeholder_nop)) == 0) {
      return HotpatchPlaceholderFormat::NOP;
    }

    if (::memcmp(reinterpret_cast<void*>(co_create_instance_padded_address_),
                 reinterpret_cast<const void*>(&g_hotpatch_placeholder_apphelp),
                 sizeof(g_hotpatch_placeholder_apphelp)) == 0) {
      return HotpatchPlaceholderFormat::APPHELP_SHIM;
    }

    const unsigned char* instruction_bytes =
        reinterpret_cast<const unsigned char*>(
            co_create_instance_padded_address_);
    const unsigned char entry_point_byte = instruction_bytes[5];
    // Check for all of the common jmp opcodes.
    if (entry_point_byte == 0xeb || entry_point_byte == 0xe9 ||
        entry_point_byte == 0xff || entry_point_byte == 0xea) {
      return HotpatchPlaceholderFormat::EXTERNALLY_PATCHED;
    }

    return HotpatchPlaceholderFormat::UNKNOWN;
  }

  bool WasHotpatchChanged() {
    if (::memcmp(reinterpret_cast<void*>(co_create_instance_padded_address_),
                 reinterpret_cast<const void*>(&structured_hotpatch_),
                 sizeof(structured_hotpatch_)) == 0) {
      return false;
    }

    NOTREACHED() << "CoCreateInstance patch overwritten. Expected: <"
                 << FirstSevenBytesToString(co_create_instance_padded_address_)
                 << ">, Actual: <"
                 << FirstSevenBytesToString(
                        reinterpret_cast<uint32_t>(&structured_hotpatch_))
                 << ">";
  }

  // Indirect call to original_co_create_instance_body_function_ triggers CFI
  // so this function must have CFI disabled.
  DISABLE_CFI_ICALL static HRESULT __stdcall DCheckedCoCreateInstance(
      const CLSID& rclsid,
      IUnknown* pUnkOuter,
      DWORD dwClsContext,
      REFIID riid,
      void** ppv) {
    // Chromium COM callers need to make sure that their thread is configured to
    // process COM objects to avoid creating an implicit MTA or silently failing
    // STA object creation call due to the SUCCEEDED() pattern for COM calls.
    //
    // If you hit this assert as part of migrating to the Task Scheduler,
    // evaluate your threading guarantees and dispatch your work with
    // base::ThreadPool::CreateCOMSTATaskRunner().
    //
    // If you need MTA support, ping //base/task/thread_pool/OWNERS.
    AssertComInitialized(
        "CoCreateInstance calls in Chromium require explicit COM "
        "initialization via base::ThreadPool::CreateCOMSTATaskRunner() or "
        "ScopedCOMInitializer. See the comment in DCheckedCoCreateInstance for "
        "more details.");
    return original_co_create_instance_body_function_(rclsid, pUnkOuter,
                                                      dwClsContext, riid, ppv);
  }

  // Returns the first 7 bytes in hex as a string at |address|.
  static std::string FirstSevenBytesToString(uint32_t address) {
    const unsigned char* bytes =
        reinterpret_cast<const unsigned char*>(address);
    return base::StringPrintf("%02x %02x %02x %02x %02x %02x %02x", bytes[0],
                              bytes[1], bytes[2], bytes[3], bytes[4], bytes[5],
                              bytes[6]);
  }

  // Synchronizes everything in this class.
  base::Lock lock_;
  size_t init_count_ = 0;
  bool disabled_ = false;
  HMODULE ole32_library_ = nullptr;
  uint32_t co_create_instance_padded_address_ = 0;
  HotpatchPlaceholderFormat hotpatch_placeholder_format_ =
      HotpatchPlaceholderFormat::UNKNOWN;
  StructuredHotpatch structured_hotpatch_;
  static decltype(
      ::CoCreateInstance)* original_co_create_instance_body_function_;
};

decltype(::CoCreateInstance)*
    HookManager::original_co_create_instance_body_function_ = nullptr;

}  // namespace

#endif  // defined(COM_INIT_CHECK_HOOK_ENABLED)

ComInitCheckHook::ComInitCheckHook() {
#if defined(COM_INIT_CHECK_HOOK_ENABLED)
  HookManager::GetInstance()->RegisterHook();
#endif  // defined(COM_INIT_CHECK_HOOK_ENABLED)
}

ComInitCheckHook::~ComInitCheckHook() {
#if defined(COM_INIT_CHECK_HOOK_ENABLED)
  HookManager::GetInstance()->UnregisterHook();
#endif  // defined(COM_INIT_CHECK_HOOK_ENABLED)
}

void ComInitCheckHook::DisableCOMChecksForProcess() {
#if defined(COM_INIT_CHECK_HOOK_ENABLED)
  HookManager::GetInstance()->DisableCOMChecksForProcess();
#endif
}

}  // namespace win
}  // namespace base
