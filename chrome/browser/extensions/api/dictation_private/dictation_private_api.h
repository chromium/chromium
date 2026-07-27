// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DICTATION_PRIVATE_DICTATION_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DICTATION_PRIVATE_DICTATION_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class DictationPrivateUpdateTranscriptionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("dictationPrivate.updateTranscription",
                             DICTATIONPRIVATE_UPDATETRANSCRIPTION)

 protected:
  ~DictationPrivateUpdateTranscriptionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class DictationPrivateSetStreamStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("dictationPrivate.setStreamState",
                             DICTATIONPRIVATE_SETSTREAMSTATE)

 protected:
  ~DictationPrivateSetStreamStateFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Implements the dictationPrivate.updateAudioLevel API function.
class DictationPrivateUpdateAudioLevelFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("dictationPrivate.updateAudioLevel",
                             DICTATIONPRIVATE_UPDATEAUDIOLEVEL)

 protected:
  ~DictationPrivateUpdateAudioLevelFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DICTATION_PRIVATE_DICTATION_PRIVATE_API_H_
