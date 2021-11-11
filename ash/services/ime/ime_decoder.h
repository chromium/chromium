// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_IME_DECODER_H_
#define ASH_SERVICES_IME_IME_DECODER_H_

#include "ash/services/ime/public/cpp/shared_lib/interfaces.h"

#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace ime {

// A proxy class for the IME decoder.
// ImeDecoder is implemented as a singleton and is initialized before 'ime'
// sandbox is engaged.
class ImeDecoder {
 public:
  // Status of loading func from IME decoder DSO: either success or error type.
  enum class Status {
    kSuccess = 0,
    kUninitialized = 1,
    kNotInstalled = 2,
    kLoadLibraryFailed = 3,
    kFunctionMissing = 4,
  };

  // This contains the function pointers to the entry points for the loaded
  // decoder shared library.
  struct EntryPoints {
    ImeDecoderInitOnceFn init_once;
    ImeDecoderSupportsFn supports;
    ImeDecoderActivateImeFn activate_ime;
    ImeDecoderProcessFn process;
    ImeDecoderCloseFn close;
    ConnectToInputMethodFn connect_to_input_method;
    IsInputMethodConnectedFn is_input_method_connected;

    // Whether the EntryPoints is ready to use.
    bool is_ready = false;
  };

  // Gets the singleton ImeDecoder.
  static ImeDecoder* GetInstance();

  ImeDecoder(const ImeDecoder&) = delete;
  ImeDecoder& operator=(const ImeDecoder&) = delete;

  // Get status of the IME decoder library initialization.
  // Return `Status::kSuccess` if the lib is successfully initialized.
  Status GetStatus() const;

  // Returns entry points of the loaded decoder shared library.
  EntryPoints GetEntryPoints();

 private:
  friend class base::NoDestructor<ImeDecoder>;

  // Initialize the Ime decoder library.
  explicit ImeDecoder();
  ~ImeDecoder();

  Status status_;

  // Result of IME decoder DSO initialization.
  absl::optional<base::ScopedNativeLibrary> library_;

  EntryPoints entry_points_;
};

// Only used in tests to set a fake `ImeDecoder::EntryPoints`.
void FakeDecoderEntryPointsForTesting(
    const ImeDecoder::EntryPoints& decoder_entry_points);

}  // namespace ime
}  // namespace chromeos

#endif  // ASH_SERVICES_IME_IME_DECODER_H_
