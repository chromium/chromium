// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/language_packs/language_packs_extensions_util.h"

#include "base/logging.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"

namespace chromeos {

extensions::api::input_method_private::LanguagePackStatus
LanguagePackResultToExtensionStatus(
    const ash::language_packs::PackResult& result) {
  using ash::language_packs::PackResult;
  namespace input_method_private = extensions::api::input_method_private;

  if (result.operation_error != PackResult::ErrorCode::kNone) {
    if (result.operation_error == PackResult::ErrorCode::kNeedReboot) {
      return input_method_private::LanguagePackStatus::kErrorNeedsReboot;
    } else {
      return input_method_private::LanguagePackStatus::kErrorOther;
    }
  }

  switch (result.pack_state) {
    case PackResult::StatusCode::kUnknown:
      return input_method_private::LanguagePackStatus::kUnknown;
    case PackResult::StatusCode::kNotInstalled:
      return input_method_private::LanguagePackStatus::kNotInstalled;
    case PackResult::StatusCode::kInProgress:
      return input_method_private::LanguagePackStatus::kInProgress;
    case PackResult::StatusCode::kInstalled:
      return input_method_private::LanguagePackStatus::kInstalled;
  }
  LOG(ERROR) << "Unexpected PackResult pack_state.";
  return input_method_private::LanguagePackStatus::kUnknown;
}

}  // namespace chromeos
