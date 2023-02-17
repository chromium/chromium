// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_H_

#include <vector>

#include "base/values.h"
#include "content/public/browser/tts_controller.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace tts_engine_events {
extern const char kOnSpeak[];
extern const char kOnSpeakWithAudioStream[];
extern const char kOnStop[];
extern const char kOnPause[];
extern const char kOnResume[];
}

// TtsEngineDelegate implementation used by TtsController.
class TtsExtensionEngine : public content::TtsEngineDelegate {
 public:
  static TtsExtensionEngine* GetInstance();

  TtsExtensionEngine();
  ~TtsExtensionEngine() override;

  // Sends audio buffer for playback in tts service. See
  // chromeos/services/tts/public/mojom for more details.
  virtual void SendAudioBuffer(int utterance_id,
                               const std::vector<float>& audio_buffer,
                               int char_index,
                               bool is_last_buffer) {}

  // Overridden from TtsEngineDelegate:
  void GetVoices(content::BrowserContext* browser_context,
                 const GURL& source_url,
                 std::vector<content::VoiceData>* out_voices) override;
  void Speak(content::TtsUtterance* utterance,
             const content::VoiceData& voice) override;
  void Stop(content::TtsUtterance* utterance) override;
  void Pause(content::TtsUtterance* utterance) override;
  void Resume(content::TtsUtterance* utterance) override;
  void LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override;
  bool IsBuiltInTtsEngineInitialized(
      content::BrowserContext* browser_context) override;

  // Stops the given speech engine loaded in |browser_context|.
  void Stop(content::BrowserContext* browser_context,
            const std::string& engine_id);
  // Pauses the given speech engine loaded in |browser_context|.
  void Pause(content::BrowserContext* browser_context,
             const std::string& engine_id);
  // Resumes the given speech engine loaded in |browser_context|.
  void Resume(content::BrowserContext* browser_context,
              const std::string& engine_id);

  void DisableBuiltInTTSEngineForTesting() {
    disable_built_in_tts_engine_for_testing_ = true;
  }

 protected:
  base::Value::List BuildSpeakArgs(content::TtsUtterance* utterance,
                                   const content::VoiceData& voice);

  bool disable_built_in_tts_engine_for_testing_ = false;
};

// Function that allows tts engines to update its list of supported voices at
// runtime.
class ExtensionTtsEngineUpdateVoicesFunction : public ExtensionFunction {
 private:
  ~ExtensionTtsEngineUpdateVoicesFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("ttsEngine.updateVoices", TTSENGINE_UPDATEVOICES)
};

// Hidden/internal extension function used to allow TTS engine extensions
// to send events back to the client that's calling tts.speak().
class ExtensionTtsEngineSendTtsEventFunction : public ExtensionFunction {
 private:
  ~ExtensionTtsEngineSendTtsEventFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("ttsEngine.sendTtsEvent", TTSENGINE_SENDTTSEVENT)
};

// Hidden/internal extension function used to allow TTS engine extensions
// to send audio back to the client that's calling tts.speak().
class ExtensionTtsEngineSendTtsAudioFunction : public ExtensionFunction {
 private:
  ~ExtensionTtsEngineSendTtsAudioFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("ttsEngine.sendTtsAudio", TTSENGINE_SENDTTSAUDIO)
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_API_H_
