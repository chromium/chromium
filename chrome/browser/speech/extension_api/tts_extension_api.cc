// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_extension_api.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_client_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace constants = tts_extension_api_constants;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

// ChromeOS source that triggered text-to-speech utterance.
//
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "TextToSpeechSource" in src/tools/metrics/histograms/enums.xml.
// LINT.IfChange(UMATextToSpeechSource)
enum class UMATextToSpeechSource {
  kOther = 0,
  kChromeVox = 1,
  kSelectToSpeak = 2,

  kMaxValue = kSelectToSpeak,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:TextToSpeechSource)

}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace events {
const char kOnEvent[] = "tts.onEvent";
const char kOnVoicesChanged[] = "tts.onVoicesChanged";
}  // namespace events

const char* TtsEventTypeToString(content::TtsEventType event_type) {
  switch (event_type) {
    case content::TTS_EVENT_START:
      return constants::kEventTypeStart;
    case content::TTS_EVENT_END:
      return constants::kEventTypeEnd;
    case content::TTS_EVENT_WORD:
      return constants::kEventTypeWord;
    case content::TTS_EVENT_SENTENCE:
      return constants::kEventTypeSentence;
    case content::TTS_EVENT_MARKER:
      return constants::kEventTypeMarker;
    case content::TTS_EVENT_INTERRUPTED:
      return constants::kEventTypeInterrupted;
    case content::TTS_EVENT_CANCELLED:
      return constants::kEventTypeCancelled;
    case content::TTS_EVENT_ERROR:
      return constants::kEventTypeError;
    case content::TTS_EVENT_PAUSE:
      return constants::kEventTypePause;
    case content::TTS_EVENT_RESUME:
      return constants::kEventTypeResume;
    default:
      NOTREACHED_IN_MIGRATION();
      return constants::kEventTypeError;
  }
}

content::TtsEventType TtsEventTypeFromString(const std::string& str) {
  if (str == constants::kEventTypeStart)
    return content::TTS_EVENT_START;
  if (str == constants::kEventTypeEnd)
    return content::TTS_EVENT_END;
  if (str == constants::kEventTypeWord)
    return content::TTS_EVENT_WORD;
  if (str == constants::kEventTypeSentence)
    return content::TTS_EVENT_SENTENCE;
  if (str == constants::kEventTypeMarker)
    return content::TTS_EVENT_MARKER;
  if (str == constants::kEventTypeInterrupted)
    return content::TTS_EVENT_INTERRUPTED;
  if (str == constants::kEventTypeCancelled)
    return content::TTS_EVENT_CANCELLED;
  if (str == constants::kEventTypeError)
    return content::TTS_EVENT_ERROR;
  if (str == constants::kEventTypePause)
    return content::TTS_EVENT_PAUSE;
  if (str == constants::kEventTypeResume)
    return content::TTS_EVENT_RESUME;

  NOTREACHED_IN_MIGRATION();
  return content::TTS_EVENT_ERROR;
}

namespace extensions {

// One of these is constructed for each utterance, and deleted
// when the utterance gets any final event.
class TtsExtensionEventHandler : public content::UtteranceEventDelegate {
 public:
  explicit TtsExtensionEventHandler(const std::string& src_extension_id);

  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override;

 private:
  // The extension ID of the extension that called speak() and should
  // receive events.
  std::string src_extension_id_;
};

TtsExtensionEventHandler::TtsExtensionEventHandler(
    const std::string& src_extension_id)
    : src_extension_id_(src_extension_id) {
}

void TtsExtensionEventHandler::OnTtsEvent(content::TtsUtterance* utterance,
                                          content::TtsEventType event_type,
                                          int char_index,
                                          int length,
                                          const std::string& error_message) {
  if (utterance->GetSrcId() < 0) {
    if (utterance->IsFinished())
      delete this;
    return;
  }

  const std::set<content::TtsEventType>& desired_event_types =
      utterance->GetDesiredEventTypes();
  if (!desired_event_types.empty() &&
      desired_event_types.find(event_type) == desired_event_types.end()) {
    if (utterance->IsFinished())
      delete this;
    return;
  }

  const char *event_type_string = TtsEventTypeToString(event_type);
  base::Value::Dict details;
  if (char_index >= 0)
    details.Set(constants::kCharIndexKey, char_index);
  if (length >= 0)
    details.Set(constants::kLengthKey, length);
  details.Set(constants::kEventTypeKey, event_type_string);
  if (event_type == content::TTS_EVENT_ERROR) {
    details.Set(constants::kErrorMessageKey, error_message);
  }
  details.Set(constants::kSrcIdKey, utterance->GetSrcId());
  details.Set(constants::kIsFinalEventKey, utterance->IsFinished());

  base::Value::List arguments;
  arguments.Append(std::move(details));

  auto event = std::make_unique<extensions::Event>(
      ::extensions::events::TTS_ON_EVENT, ::events::kOnEvent,
      std::move(arguments), utterance->GetBrowserContext());
  event->event_url = utterance->GetSrcUrl();
  extensions::EventRouter::Get(utterance->GetBrowserContext())
      ->DispatchEventToExtension(src_extension_id_, std::move(event));

  if (utterance->IsFinished())
    delete this;
}

ExtensionFunction::ResponseAction TtsSpeakFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& text = args()[0].GetString();
  if (text.size() > 32768) {
    return RespondNow(Error(constants::kErrorUtteranceTooLong));
  }

  base::Value::Dict options;
  if (args().size() >= 2 && args()[1].is_dict())
    options = args()[1].GetDict().Clone();

  std::string voice_name;
  if (base::Value* voice_name_value = options.Find(constants::kVoiceNameKey)) {
    EXTENSION_FUNCTION_VALIDATE(voice_name_value->is_string());
    voice_name = voice_name_value->GetString();
  }

  std::string lang;
  if (base::Value* lang_value = options.Find(constants::kLangKey)) {
    EXTENSION_FUNCTION_VALIDATE(lang_value->is_string());
    lang = lang_value->GetString();
  }
  if (!lang.empty() && !l10n_util::IsValidLocaleSyntax(lang)) {
    return RespondNow(Error(constants::kErrorInvalidLang));
  }

  double rate = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  if (base::Value* rate_value = options.Find(constants::kRateKey)) {
    EXTENSION_FUNCTION_VALIDATE(rate_value->GetIfDouble());
    rate = rate_value->GetIfDouble().value_or(rate);
    if (rate < 0.1 || rate > 10.0) {
      return RespondNow(Error(constants::kErrorInvalidRate));
    }
  }

  double pitch = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  if (base::Value* pitch_value = options.Find(constants::kPitchKey)) {
    EXTENSION_FUNCTION_VALIDATE(pitch_value->GetIfDouble());
    pitch = pitch_value->GetIfDouble().value_or(pitch);
    if (pitch < 0.0 || pitch > 2.0) {
      return RespondNow(Error(constants::kErrorInvalidPitch));
    }
  }

  double volume = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  if (base::Value* volume_value = options.Find(constants::kVolumeKey)) {
    EXTENSION_FUNCTION_VALIDATE(volume_value->GetIfDouble());
    volume = volume_value->GetIfDouble().value_or(volume);
    if (volume < 0.0 || volume > 1.0) {
      return RespondNow(Error(constants::kErrorInvalidVolume));
    }
  }

  bool can_enqueue = options.FindBool(constants::kEnqueueKey).value_or(false);
  if (base::Value* value = options.Find(constants::kEnqueueKey)) {
    EXTENSION_FUNCTION_VALIDATE(value->is_bool());
  }

  std::set<content::TtsEventType> required_event_types;
  if (options.contains(constants::kRequiredEventTypesKey)) {
    base::Value::List* list =
        options.FindList(constants::kRequiredEventTypesKey);
    EXTENSION_FUNCTION_VALIDATE(list);
    for (const base::Value& i : *list) {
      const std::string* event_type = i.GetIfString();
      if (event_type) {
        required_event_types.insert(
            TtsEventTypeFromString(event_type->c_str()));
      }
    }
  }

  std::set<content::TtsEventType> desired_event_types;
  if (options.contains(constants::kDesiredEventTypesKey)) {
    base::Value::List* list =
        options.FindList(constants::kDesiredEventTypesKey);
    EXTENSION_FUNCTION_VALIDATE(list);
    for (const base::Value& i : *list) {
      const std::string* event_type = i.GetIfString();
      if (event_type)
        desired_event_types.insert(TtsEventTypeFromString(event_type->c_str()));
    }
  }

  std::string voice_extension_id;
  if (base::Value* voice_extension_id_value =
          options.Find(constants::kExtensionIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(voice_extension_id_value);
    voice_extension_id = voice_extension_id_value->GetString();
  }

  int src_id = -1;
  base::Value* src_id_value = options.Find(constants::kSrcIdKey);
  if (src_id_value) {
    EXTENSION_FUNCTION_VALIDATE(src_id_value->is_int());
    src_id = src_id_value->GetInt();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  UMATextToSpeechSource source = UMATextToSpeechSource::kOther;
  const std::string host = source_url().host();
  if (host == extension_misc::kSelectToSpeakExtensionId) {
    source = UMATextToSpeechSource::kSelectToSpeak;
  } else if (host == extension_misc::kChromeVoxExtensionId) {
    source = UMATextToSpeechSource::kChromeVox;
  }
  UMA_HISTOGRAM_ENUMERATION("TextToSpeech.Utterance.Source", source);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // If we got this far, the arguments were all in the valid format, so
  // send the success response to the callback now - this ensures that
  // the callback response always arrives before events, which makes
  // the behavior more predictable and easier to write unit tests for too.
  Respond(NoArguments());

  std::unique_ptr<content::TtsUtterance> utterance;
  if (extension()) {
    extensions::ExtensionHost* extension_host =
        extensions::ProcessManager::Get(browser_context())
            ->GetBackgroundHostForExtension(extension()->id());

    if (extension_host && extension_host->host_contents()) {
      utterance =
          content::TtsUtterance::Create(extension_host->host_contents());
    }
  }

  if (!utterance)
    utterance = content::TtsUtterance::Create(browser_context());

  utterance->SetText(text);
  utterance->SetVoiceName(voice_name);
  utterance->SetSrcId(src_id);
  utterance->SetSrcUrl(source_url());
  utterance->SetLang(lang);
  utterance->SetContinuousParameters(rate, pitch, volume);
  utterance->SetShouldClearQueue(!can_enqueue);
  utterance->SetRequiredEventTypes(required_event_types);
  utterance->SetDesiredEventTypes(desired_event_types);
  utterance->SetEngineId(voice_extension_id);
  utterance->SetOptions(std::move(options));
  if (extension())
    utterance->SetEventDelegate(new TtsExtensionEventHandler(extension_id()));

  content::TtsController* controller = content::TtsController::GetInstance();
  controller->SpeakOrEnqueue(std::move(utterance));
  return AlreadyResponded();
}

ExtensionFunction::ResponseAction TtsStopSpeakingFunction::Run() {
  content::TtsController::GetInstance()->Stop(source_url());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TtsPauseFunction::Run() {
  content::TtsController::GetInstance()->Pause();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TtsResumeFunction::Run() {
  content::TtsController::GetInstance()->Resume();
  return RespondNow(NoArguments());
}

void TtsIsSpeakingFunction::OnIsSpeakingComplete(bool speaking) {
  Respond(WithArguments(speaking));
}

ExtensionFunction::ResponseAction TtsIsSpeakingFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros tts support is behind an ash feature flag and pushed to Lacros via
  // crosapi. The feature flag is disabled by default and can not be turned on
  // in ash from lacros browser test. To enable lacros tts support for lacros
  // browser test, we have to use a workaround to enable it for testing.
  // TtsPlatformImplLacros::PlatformImplSupported() returns true if lacros
  // tts support is enabled either by ash feature flag or by testing workaround.
  // TODO(crbug.com/40259646): Remove the workaround for enable lacros tts
  // support for testing and call
  // tts_crosapi_util::ShouldEnableLacrosTtsSupport() instead.
  if (content::TtsPlatform::GetInstance()->PlatformImplSupported()) {
    content::BrowserContext* browser_context =
        ProfileManager::GetPrimaryUserProfile();
    TtsClientLacros::GetForBrowserContext(browser_context)
        ->IsSpeaking(
            base::BindOnce(&TtsIsSpeakingFunction::OnIsSpeakingComplete, this));

    return RespondLater();
  }
#endif

  return RespondNow(
      WithArguments(content::TtsController::GetInstance()->IsSpeaking()));
}

ExtensionFunction::ResponseAction TtsGetVoicesFunction::Run() {
  std::vector<content::VoiceData> voices;
  content::TtsController::GetInstance()->GetVoices(browser_context(),
                                                   source_url(), &voices);

  base::Value::List result_voices;
  for (size_t i = 0; i < voices.size(); ++i) {
    const content::VoiceData& voice = voices[i];
    base::Value::Dict result_voice;
    result_voice.Set(constants::kVoiceNameKey, voice.name);
    result_voice.Set(constants::kRemoteKey, voice.remote);
    if (!voice.lang.empty())
      result_voice.Set(constants::kLangKey, voice.lang);
    if (!voice.engine_id.empty())
      result_voice.Set(constants::kExtensionIdKey, voice.engine_id);

    base::Value::List event_types;
    for (auto& event : voice.events) {
      const char* event_name_constant = TtsEventTypeToString(event);
      event_types.Append(event_name_constant);
    }
    result_voice.Set(constants::kEventTypesKey, std::move(event_types));

    result_voices.Append(std::move(result_voice));
  }

  return RespondNow(WithArguments(std::move(result_voices)));
}

TtsAPI::TtsAPI(content::BrowserContext* context) {
  ExtensionFunctionRegistry& registry =
      ExtensionFunctionRegistry::GetInstance();
  registry.RegisterFunction<ExtensionTtsEngineUpdateVoicesFunction>();
  registry.RegisterFunction<ExtensionTtsEngineSendTtsEventFunction>();
  registry.RegisterFunction<ExtensionTtsEngineSendTtsAudioFunction>();
  registry.RegisterFunction<TtsGetVoicesFunction>();
  registry.RegisterFunction<TtsIsSpeakingFunction>();
  registry.RegisterFunction<TtsSpeakFunction>();
  registry.RegisterFunction<TtsStopSpeakingFunction>();
  registry.RegisterFunction<TtsPauseFunction>();
  registry.RegisterFunction<TtsResumeFunction>();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ensure we're observing newly added engines for the given context.
  TtsEngineExtensionObserverChromeOSFactory::GetForProfile(
      Profile::FromBrowserContext(context));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);

  event_router_ = EventRouter::Get(context);
  event_router_->RegisterObserver(this, ::events::kOnVoicesChanged);
}

TtsAPI::~TtsAPI() {
  content::TtsController::GetInstance()->RemoveVoicesChangedDelegate(this);
  event_router_->UnregisterObserver(this);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<TtsAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<TtsAPI>* TtsAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void TtsAPI::OnVoicesChanged() {
  if (!broadcast_events_) {
    return;
  }
  auto event = std::make_unique<extensions::Event>(
      events::TTS_ON_VOICES_CHANGED, ::events::kOnVoicesChanged,
      base::Value::List());
  event_router_->BroadcastEvent(std::move(event));
}

void TtsAPI::OnListenerAdded(const EventListenerInfo& details) {
  StartOrStopListeningForVoicesChanged();
}

void TtsAPI::OnListenerRemoved(const EventListenerInfo& details) {
  StartOrStopListeningForVoicesChanged();
}

void TtsAPI::StartOrStopListeningForVoicesChanged() {
  broadcast_events_ =
      event_router_->HasEventListener(::events::kOnVoicesChanged);
}

}  // namespace extensions
