// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/ime/ime_decoder.h"

#include "ash/constants/ash_features.h"
#include "ash/services/ime/constants.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace chromeos {
namespace ime {

namespace {

absl::optional<ImeDecoder::EntryPoints> g_fake_decoder_entry_points_for_testing;

const char kCrosImeDecoderLib[] = "libimedecoder.so";

// TODO(b/161491092): Add test image path based on value of
// "CHROMEOS_RELEASE_TRACK" from `base::SysInfo::GetLsbReleaseValue`.
// Returns ImeDecoderLib path based on the run time env.
base::FilePath GetImeDecoderLibPath() {
#if defined(__x86_64__) || defined(__aarch64__)
  base::FilePath lib_path("/usr/lib64");
#else
  base::FilePath lib_path("/usr/lib");
#endif
  lib_path = lib_path.Append(kCrosImeDecoderLib);
  return lib_path;
}

// Simple bridge between logging in the loaded shared library and logging in
// Chrome.
void ImeLoggerBridge(int severity, const char* message) {
  switch (severity) {
    case logging::LOG_INFO:
      // TODO(b/162375823): VLOG_IF(INFO, is_debug_version).
      break;
    case logging::LOG_WARNING:
      LOG(WARNING) << message;
      break;
    case logging::LOG_ERROR:
      LOG(ERROR) << message;
      break;
    default:
      break;
  }
}

}  // namespace

ImeDecoder::ImeDecoder() {
  if (g_fake_decoder_entry_points_for_testing) {
    entry_points_ = g_fake_decoder_entry_points_for_testing;
    return;
  }

  base::FilePath path = GetImeDecoderLibPath();

  // Add dlopen flags (RTLD_LAZY | RTLD_NODELETE) later.
  base::ScopedNativeLibrary library = base::ScopedNativeLibrary(path);
  if (!library.is_valid()) {
    LOG(ERROR) << "Failed to load decoder shared library from: " << path
               << ", error: " << library.GetError()->ToString();
    return;
  }

  EntryPoints entry_points;
  entry_points.init_once = reinterpret_cast<ImeDecoderInitOnceFn>(
      library.GetFunctionPointer(kImeDecoderInitOnceFnName));
  entry_points.supports = reinterpret_cast<ImeDecoderSupportsFn>(
      library.GetFunctionPointer(kImeDecoderSupportsFnName));
  entry_points.activate_ime = reinterpret_cast<ImeDecoderActivateImeFn>(
      library.GetFunctionPointer(kImeDecoderActivateImeFnName));
  entry_points.process = reinterpret_cast<ImeDecoderProcessFn>(
      library.GetFunctionPointer(kImeDecoderProcessFnName));
  entry_points.close = reinterpret_cast<ImeDecoderCloseFn>(
      library.GetFunctionPointer(kImeDecoderCloseFnName));
  entry_points.connect_to_input_method =
      reinterpret_cast<ConnectToInputMethodFn>(
          library.GetFunctionPointer(kConnectToInputMethodFnName));
  entry_points.is_input_method_connected =
      reinterpret_cast<IsInputMethodConnectedFn>(
          library.GetFunctionPointer(kIsInputMethodConnectedFnName));

  // Checking if entry_points_ are loaded.
  if (!entry_points.init_once || !entry_points.supports ||
      !entry_points.activate_ime || !entry_points.process ||
      !entry_points.close) {
    return;
  }

  // Optional function pointer.
  SetImeEngineLoggerFn logger_setter = reinterpret_cast<SetImeEngineLoggerFn>(
      library.GetFunctionPointer(kSetImeEngineLoggerFnName));
  if (logger_setter) {
    logger_setter(ImeLoggerBridge);
  } else {
    LOG(WARNING) << "Failed to set a Chrome Logger for decoder DSO.";
  }

  library_ = std::move(library);
  entry_points_ = entry_points;
}

ImeDecoder::~ImeDecoder() = default;

ImeDecoder* ImeDecoder::GetInstance() {
  static base::NoDestructor<ImeDecoder> instance;
  return instance.get();
}

absl::optional<ImeDecoder::EntryPoints> ImeDecoder::GetEntryPoints() const {
  return entry_points_;
}

void FakeDecoderEntryPointsForTesting(  // IN-TEST
    const ImeDecoder::EntryPoints& decoder_entry_points) {
  g_fake_decoder_entry_points_for_testing = decoder_entry_points;
}

}  // namespace ime
}  // namespace chromeos
