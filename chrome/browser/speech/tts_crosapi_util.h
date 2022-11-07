// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_CROSAPI_UTIL_H_
#define CHROME_BROWSER_SPEECH_TTS_CROSAPI_UTIL_H_

#include <memory>
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"

namespace tts_crosapi_util {

// Functions for converting Tts data types to/from its corresponding mojo types.
content::TtsEventType FromMojo(crosapi::mojom::TtsEventType mojo_event);
crosapi::mojom::TtsEventType ToMojo(content::TtsEventType event_type);
content::VoiceData FromMojo(const crosapi::mojom::TtsVoicePtr& mojo_voice);
crosapi::mojom::TtsVoicePtr ToMojo(const content::VoiceData& voice);
crosapi::mojom::TtsUtterancePtr ToMojo(content::TtsUtterance* utterance);
std::unique_ptr<content::TtsUtterance> FromMojo(
    crosapi::mojom::TtsUtterancePtr& mojo_utterance);

bool ShouldEnableLacrosTtsSupport();

// Functions for testing use only.

// This function allows StandaloneBrowserTestController located in
// chrome/browser/lacros to retrieve voice data via
// content::TtsController::GetVoices(), which can not be called directly
// from StandaloneBrowserTestController since chrome/browser/DEPS disallows
// content/public/browser/tts_controller.h explicitly.
void GetAllVoicesForTesting(content::BrowserContext* browser_context,
                            const GURL& source_url,
                            std::vector<content::VoiceData>* out_voices);

// This function allows ash browser test to call StandaloneBrowserTestController
// to speak a Lacros utterance, which is a workaround for crbug/1368284.
void SpeakForTesting(std::unique_ptr<content::TtsUtterance> utterance);
}  // namespace tts_crosapi_util

#endif  // CHROME_BROWSER_SPEECH_TTS_CROSAPI_UTIL_H_
