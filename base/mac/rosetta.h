// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_ROSETTA_H_
#define BASE_MAC_ROSETTA_H_

#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

namespace base {

class FilePath;

namespace mac {

#if defined(ARCH_CPU_X86_64)

// Returns true if the current process is being translated by Rosetta.
bool ProcessIsTranslated();

#endif  // ARCH_CPU_X86_64

#if defined(ARCH_CPU_ARM64)

// Returns true if Rosetta is installed and available to translate x86_64 code.
BASE_EXPORT bool IsRosettaInstalled();

// Prompt the user to allow for the installation of Rosetta. `callback` is
// called with the result of the Rosetta installation. The UI is presented to
// the user in a dialog with the `title_text` and `body_text`. Its thread-safety
// is not known; call it from the main thread and the callback will happen on
// the main thread as well.
enum class RosettaInstallationResult {
  kFailedToAccessSPI,
  kAlreadyInstalled,
  kInstallationFailure,
  kInstallationSuccess,
};
BASE_EXPORT void RequestRosettaInstallation(
    const string16& title_text,
    const string16& body_text,
    OnceCallback<void(RosettaInstallationResult)> callback);

#endif  // ARCH_CPU_ARM64

// Requests an ahead-of-time translation of the binaries with paths given in
// `binaries`. Returns the success value (true == success, false == failure)
// indicated by the underlying call.
//
// Observed behavior about Rosetta AOT translation:
// - If a binary was already translated, it will not be translated again.
// - The call blocks and waits for the completion of the translation. Do not
//   call this on the main thread.
BASE_EXPORT bool RequestRosettaAheadOfTimeTranslation(
    const std::vector<FilePath>& binaries);

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_ROSETTA_H_
