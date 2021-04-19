// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_CHROMEOS_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_CHROMEOS_H_

#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"

namespace content {
class BrowserContext;
}

// TtsEngineDelegate implementation used by TtsController on Chrome OS.
class TtsExtensionEngineChromeOS : public TtsExtensionEngine {
 public:
  // Overridden from TtsEngineDelegate:
  void LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override;
  bool IsBuiltInTtsEngineInitialized(
      content::BrowserContext* browser_context) override;
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_CHROMEOS_H_
