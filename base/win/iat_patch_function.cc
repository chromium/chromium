// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/iat_patch_function.h"

#include "base/check_op.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/win/patch_util.h"
#include "base/win/pe_image.h"

namespace base {
namespace win {

namespace {

struct InterceptFunctionInformation {
  bool finished_operation;
  const char* imported_from_module;
  const char* function_name;
  // RAW_PTR_EXCLUSION: #reinterpret-cast-trivial-type
  RAW_PTR_EXCLUSION void* new_function;
  RAW_PTR_EXCLUSION void** old_function;
  RAW_PTR_EXCLUSION IMAGE_THUNK_DATA** iat_thunk;
  DWORD return_code;
};

void* GetIATFunction(IMAGE_THUNK_DATA* iat_thunk) {
  if (!iat_thunk) {
    NOTREACHED();
  }

  // Works around the 64 bit portability warning:
  // The Function member inside IMAGE_THUNK_DATA is really a pointer
  // to the IAT function. IMAGE_THUNK_DATA correctly maps to IMAGE_THUNK_DATA32
  // or IMAGE_THUNK_DATA64 for correct pointer size.
  union FunctionThunk {
    IMAGE_THUNK_DATA thunk;
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #union
    RAW_PTR_EXCLUSION void* pointer;
  } iat_function;

  iat_function.thunk = *iat_thunk;
  return iat_function.pointer;
}

bool InterceptEnumCallback(const base::win::PEImage& image,
                           const char* module,
                           DWORD ordinal,
                           const char* name,
                           DWORD hint,
                           IMAGE_THUNK_DATA* iat,
                           void* cookie) {
  InterceptFunctionInformation* intercept_information =
      reinterpret_cast<InterceptFunctionInformation*>(cookie);

  if (!intercept_information) {
    NOTREACHED();
  }

  DCHECK(module);

  if (name && (0 == lstrcmpiA(name, intercept_information->function_name))) {
    // Save the old pointer.
    if (intercept_information->old_function) {
      *(intercept_information->old_function) = GetIATFunction(iat);
    }

    if (intercept_information->iat_thunk) {
      *(intercept_information->iat_thunk) = iat;
    }

    // portability check
    static_assert(
        sizeof(iat->u1.Function) == sizeof(intercept_information->new_function),
        "unknown IAT thunk format");

    // Patch the function.
    intercept_information->return_code = internal::ModifyCode(
        &(iat->u1.Function), &(intercept_information->new_function),
        sizeof(intercept_information->new_function));

    // Terminate further enumeration.
    intercept_information->finished_operation = true;
    return false;
  }

  return true;
}

// Helper to intercept a function in an import table of a specific
// module.
//
// Arguments:
// module_handle          Module to be intercepted
// imported_from_module   Module that exports the symbol
// function_name          Name of the API to be intercepted
// new_function           Interceptor function
// old_function           Receives the original function pointer
// iat_thunk              Receives pointer to IAT_THUNK_DATA
//                        for the API from the import table.
//
// Returns: Returns NO_ERROR on success or Windows error code
//          as defined in winerror.h
DWORD InterceptImportedFunction(HMODULE module_handle,
                                const char* imported_from_module,
                                const char* function_name,
                                void* new_function,
                                void** old_function,
                                IMAGE_THUNK_DATA** iat_thunk) {
  if (!module_handle || !imported_from_module || !function_name ||
      !new_function) {
    NOTREACHED();
  }

  base::win::PEImage target_image(module_handle);
  if (!target_image.VerifyMagic()) {
    NOTREACHED();
  }

  InterceptFunctionInformation intercept_information = {false,
                                                        imported_from_module,
                                                        function_name,
                                                        new_function,
                                                        old_function,
                                                        iat_thunk,
                                                        ERROR_GEN_FAILURE};

  // First go through the IAT. If we don't find the import we are looking
  // for in IAT, search delay import table.
  target_image.EnumAllImports(InterceptEnumCallback, &intercept_information,
                              imported_from_module);
  if (!intercept_information.finished_operation) {
    target_image.EnumAllDelayImports(
        InterceptEnumCallback, &intercept_information, imported_from_module);
  }

  return intercept_information.return_code;
}

// Restore intercepted IAT entry with the original function.
//
// Arguments:
// intercept_function     Interceptor function
// original_function      Receives the original function pointer
//
// Returns: Returns NO_ERROR on success or Windows error code
//          as defined in winerror.h
DWORD RestoreImportedFunction(void* intercept_function,
                              void* original_function,
                              IMAGE_THUNK_DATA* iat_thunk) {
  if (!intercept_function || !original_function || !iat_thunk) {
    NOTREACHED();
  }

  if (GetIATFunction(iat_thunk) != intercept_function) {
    // Check if someone else has intercepted on top of us.
    // We cannot unpatch in this case, just raise a red flag.
    NOTREACHED();
  }

  return internal::ModifyCode(&(iat_thunk->u1.Function), &original_function,
                              sizeof(original_function));
}

}  // namespace

IATPatchFunction::IATPatchFunction() = default;

IATPatchFunction::~IATPatchFunction() {
  if (intercept_function_) {
    DWORD error = Unpatch();
    DCHECK_EQ(static_cast<DWORD>(NO_ERROR), error);
  }
}

DWORD IATPatchFunction::Patch(const wchar_t* module,
                              const char* imported_from_module,
                              const char* function_name,
                              void* new_function) {
  HMODULE module_handle = LoadLibraryW(module);
  if (!module_handle) {
    NOTREACHED();
  }

  DWORD error = PatchFromModule(module_handle, imported_from_module,
                                function_name, new_function);
  if (NO_ERROR == error) {
    module_handle_ = module_handle;
  } else {
    FreeLibrary(module_handle);
  }

  return error;
}

DWORD IATPatchFunction::PatchFromModule(HMODULE module,
                                        const char* imported_from_module,
                                        const char* function_name,
                                        void* new_function) {
  DCHECK_EQ(nullptr, original_function_);
  DCHECK_EQ(nullptr, iat_thunk_);
  DCHECK_EQ(nullptr, intercept_function_);
  DCHECK(module);

  DWORD error = InterceptImportedFunction(
      module, imported_from_module, function_name, new_function,
      &original_function_.AsEphemeralRawAddr(),
      &iat_thunk_.AsEphemeralRawAddr());

  if (NO_ERROR == error) {
    DCHECK_NE(original_function_, intercept_function_);
    intercept_function_ = new_function;
  }

  return error;
}

DWORD IATPatchFunction::Unpatch() {
  DWORD error = RestoreImportedFunction(intercept_function_, original_function_,
                                        iat_thunk_);
  DCHECK_EQ(static_cast<DWORD>(NO_ERROR), error);

  // Hands off the intercept if we fail to unpatch.
  // If IATPatchFunction::Unpatch fails during RestoreImportedFunction
  // it means that we cannot safely unpatch the import address table
  // patch. In this case its better to be hands off the intercept as
  // trying to unpatch again in the destructor of IATPatchFunction is
  // not going to be any safer
  if (module_handle_)
    FreeLibrary(module_handle_);
  module_handle_ = nullptr;
  intercept_function_ = nullptr;
  original_function_ = nullptr;
  iat_thunk_ = nullptr;

  return error;
}

void* IATPatchFunction::original_function() const {
  DCHECK(is_patched());
  return original_function_;
}

}  // namespace win
}  // namespace base
