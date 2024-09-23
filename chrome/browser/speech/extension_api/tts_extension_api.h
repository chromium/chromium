// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/tts_controller.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

const char* TtsEventTypeToString(content::TtsEventType event_type);
content::TtsEventType TtsEventTypeFromString(const std::string& str);

namespace extensions {

class TtsSpeakFunction : public ExtensionFunction {
 private:
  ~TtsSpeakFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.speak", TTS_SPEAK)
};

class TtsStopSpeakingFunction : public ExtensionFunction {
 private:
  ~TtsStopSpeakingFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.stop", TTS_STOP)
};

class TtsPauseFunction : public ExtensionFunction {
 private:
  ~TtsPauseFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.pause", TTS_PAUSE)
};

class TtsResumeFunction : public ExtensionFunction {
 private:
  ~TtsResumeFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.resume", TTS_RESUME)
};

class TtsIsSpeakingFunction : public ExtensionFunction {
 private:
  ~TtsIsSpeakingFunction() override {}
  void OnIsSpeakingComplete(bool speaking);
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.isSpeaking", TTS_ISSPEAKING)
};

class TtsGetVoicesFunction : public ExtensionFunction {
 private:
  ~TtsGetVoicesFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("tts.getVoices", TTS_GETVOICES)
};

class TtsAPI : public BrowserContextKeyedAPI,
               public EventRouter::Observer,
               public content::VoicesChangedDelegate {
 public:
  explicit TtsAPI(content::BrowserContext* context);
  ~TtsAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<TtsAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<TtsAPI>;

  // VoicesChangedDelegate:
  void OnVoicesChanged() override;

  // EventRouter::Observer:
  // We will only broadcast voices changed if we have any listeners.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  void StartOrStopListeningForVoicesChanged();

  raw_ptr<EventRouter> event_router_;
  bool broadcast_events_ = false;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "TtsAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
};

template <>
struct BrowserContextFactoryDependencies<TtsAPI> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<TtsAPI>* factory) {
    factory->DependsOn(EventRouterFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_EXTENSION_API_H_
