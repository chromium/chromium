// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/extensions/api/speech/tts_engine_manifest_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/base/network_change_notifier.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/speech/tts_client_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

using extensions::EventRouter;
using extensions::Extension;
using extensions::ExtensionSystem;

namespace constants = tts_extension_api_constants;

namespace tts_engine_events {
const char kOnSpeak[] = "ttsEngine.onSpeak";
const char kOnSpeakWithAudioStream[] = "ttsEngine.onSpeakWithAudioStream";
const char kOnStop[] = "ttsEngine.onStop";
const char kOnPause[] = "ttsEngine.onPause";
const char kOnResume[] = "ttsEngine.onResume";
}  // namespace tts_engine_events

namespace {

// An extension preference to keep track of the TTS voices that a
// TTS engine extension makes available.
const char kPrefTtsVoices[] = "tts_voices";

void WarnIfMissingPauseOrResumeListener(Profile* profile,
                                        EventRouter* event_router,
                                        std::string extension_id) {
  bool has_onpause = event_router->ExtensionHasEventListener(
      extension_id, tts_engine_events::kOnPause);
  bool has_onresume = event_router->ExtensionHasEventListener(
      extension_id, tts_engine_events::kOnResume);
  if (has_onpause == has_onresume)
    return;

  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(profile)->GetBackgroundHostForExtension(
          extension_id);
  host->host_contents()->GetPrimaryMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      constants::kErrorMissingPauseOrResume);
}

std::unique_ptr<std::vector<extensions::TtsVoice>>
ValidateAndConvertToTtsVoiceVector(const extensions::Extension* extension,
                                   const base::Value::List& voices_data,
                                   bool return_after_first_error,
                                   const char** error) {
  auto tts_voices = std::make_unique<std::vector<extensions::TtsVoice>>();
  for (size_t i = 0; i < voices_data.size(); i++) {
    extensions::TtsVoice voice;
    const base::Value::Dict& voice_data = voices_data[i].GetDict();

    // Note partial validation of these attributes occurs based on tts engine's
    // json schema (e.g. for data type matching). The missing checks follow
    // similar checks in manifest parsing.
    if (const std::string* voice_name =
            voice_data.FindString(constants::kVoiceNameKey)) {
      voice.voice_name = *voice_name;
    }
    if (const base::Value* lang = voice_data.Find(constants::kLangKey)) {
      voice.lang = lang->is_string() ? lang->GetString() : std::string();
      if (!l10n_util::IsValidLocaleSyntax(voice.lang)) {
        *error = constants::kErrorInvalidLang;
        if (return_after_first_error) {
          tts_voices->clear();
          return tts_voices;
        }
        continue;
      }
    }
    if (std::optional<bool> remote =
            voice_data.FindBool(constants::kRemoteKey)) {
      voice.remote = remote.value();
    }
    if (const base::Value* extension_id_val =
            voice_data.Find(constants::kExtensionIdKey)) {
      // Allow this for clients who might have used |chrome.tts.getVoices| to
      // update existing voices. However, trying to update the voice of another
      // extension should trigger an error.
      std::string extension_id;
      if (extension_id_val->is_string())
        extension_id = extension_id_val->GetString();
      if (extension->id() != extension_id) {
        *error = constants::kErrorExtensionIdMismatch;
        if (return_after_first_error) {
          tts_voices->clear();
          return tts_voices;
        }
        continue;
      }
    }
    const base::Value::List* event_types =
        voice_data.FindList(constants::kEventTypesKey);

    if (event_types) {
      for (const auto& type : *event_types) {
        std::string event_type;
        if (type.is_string())
          event_type = type.GetString();
        voice.event_types.insert(event_type);
      }
    }

    tts_voices->push_back(voice);
  }
  return tts_voices;
}

// Get the voices for an extension, checking the preferences first
// (in case the extension has ever called UpdateVoices in the past),
// and the manifest second.
std::unique_ptr<std::vector<extensions::TtsVoice>> GetVoicesInternal(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  // First try to get the saved set of voices from extension prefs.
  auto* extension_prefs = extensions::ExtensionPrefs::Get(context);
  const base::Value::List* voices_data =
      extension_prefs->ReadPrefAsList(extension->id(), kPrefTtsVoices);
  if (voices_data) {
    const char* error = nullptr;
    return ValidateAndConvertToTtsVoiceVector(
        extension, *voices_data, /*return_after_first_error=*/false, &error);
  }

  // Fall back on the extension manifest.
  auto* manifest_voices = extensions::TtsVoices::GetTtsVoices(extension);
  if (manifest_voices) {
    return std::make_unique<std::vector<extensions::TtsVoice>>(
        *manifest_voices);
  }
  return std::make_unique<std::vector<extensions::TtsVoice>>();
}

bool GetTtsEventType(const std::string& event_type_string,
                     content::TtsEventType* event_type) {
  if (event_type_string == constants::kEventTypeStart) {
    *event_type = content::TTS_EVENT_START;
  } else if (event_type_string == constants::kEventTypeEnd) {
    *event_type = content::TTS_EVENT_END;
  } else if (event_type_string == constants::kEventTypeWord) {
    *event_type = content::TTS_EVENT_WORD;
  } else if (event_type_string == constants::kEventTypeSentence) {
    *event_type = content::TTS_EVENT_SENTENCE;
  } else if (event_type_string == constants::kEventTypeMarker) {
    *event_type = content::TTS_EVENT_MARKER;
  } else if (event_type_string == constants::kEventTypeError) {
    *event_type = content::TTS_EVENT_ERROR;
  } else if (event_type_string == constants::kEventTypePause) {
    *event_type = content::TTS_EVENT_PAUSE;
  } else if (event_type_string == constants::kEventTypeResume) {
    *event_type = content::TTS_EVENT_RESUME;
  } else {
    return false;
  }
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

bool CanUseEnhancedNetworkVoices(const GURL& source_url, Profile* profile) {
  // Currently only Select-to-speak and its settings page can use Enhanced
  // Network voices.
  if (source_url.host() != extension_misc::kSelectToSpeakExtensionId &&
      source_url != chrome::GetOSSettingsUrl(
                        chromeos::settings::mojom::kSelectToSpeakSubpagePath))
    return false;

  // Check if these voices are disallowed by policy.
  if (!profile->GetPrefs()->GetBoolean(
          ash::prefs::
              kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed)) {
    return false;
  }

  // Return true if they were enabled by the user.
  return profile->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySelectToSpeakEnhancedNetworkVoices);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TtsExtensionEngine* TtsExtensionEngine::GetInstance() {
  static base::NoDestructor<TtsExtensionEngine> tts_extension_engine;
  return tts_extension_engine.get();
}
#endif

TtsExtensionEngine::TtsExtensionEngine() = default;

TtsExtensionEngine::~TtsExtensionEngine() = default;

void TtsExtensionEngine::GetVoices(
    content::BrowserContext* browser_context,
    const GURL& source_url,
    std::vector<content::VoiceData>* out_voices) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  EventRouter* event_router = EventRouter::Get(profile);
  DCHECK(event_router);

  bool is_offline = (net::NetworkChangeNotifier::GetConnectionType() ==
                     net::NetworkChangeNotifier::CONNECTION_NONE);

  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  extensions::ExtensionSet::const_iterator iter;
  for (iter = extensions.begin(); iter != extensions.end(); ++iter) {
    const Extension* extension = iter->get();

    // A valid tts engine should have a speak and a stop listener. Either speak
    // variant is acceptable.
    if ((!event_router->ExtensionHasEventListener(
             extension->id(), tts_engine_events::kOnSpeak) &&
         !event_router->ExtensionHasEventListener(
             extension->id(), tts_engine_events::kOnSpeakWithAudioStream)) ||
        !event_router->ExtensionHasEventListener(extension->id(),
                                                 tts_engine_events::kOnStop)) {
      continue;
    }

    auto tts_voices = GetVoicesInternal(profile, extension);
    if (!tts_voices)
      continue;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Only authorized sources can use Enhanced Network voices.
    if (extension->id() == extension_misc::kEnhancedNetworkTtsExtensionId &&
        !CanUseEnhancedNetworkVoices(source_url, profile))
      continue;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    for (size_t i = 0; i < tts_voices->size(); ++i) {
      const extensions::TtsVoice& voice = tts_voices->at(i);

      // Don't return remote voices when the system is offline.
      if (voice.remote && is_offline)
        continue;

      out_voices->push_back(content::VoiceData());
      content::VoiceData& result_voice = out_voices->back();

      result_voice.native = false;
      result_voice.name = voice.voice_name;
      result_voice.lang = voice.lang;
      result_voice.remote = voice.remote;
      result_voice.engine_id = extension->id();

      for (auto it = voice.event_types.begin(); it != voice.event_types.end();
           ++it) {
        result_voice.events.insert(TtsEventTypeFromString(*it));
      }

      // If the extension sends end events, the controller will handle
      // queueing and send interrupted and cancelled events.
      if (voice.event_types.find(constants::kEventTypeEnd) !=
          voice.event_types.end()) {
        result_voice.events.insert(content::TTS_EVENT_CANCELLED);
        result_voice.events.insert(content::TTS_EVENT_INTERRUPTED);
      }
    }
  }
}

void TtsExtensionEngine::Speak(content::TtsUtterance* utterance,
                               const content::VoiceData& voice) {
  base::Value::List args = BuildSpeakArgs(utterance, voice);
  Profile* profile =
      Profile::FromBrowserContext(utterance->GetBrowserContext());
  extensions::EventRouter* event_router = EventRouter::Get(profile);
  const auto& engine_id = utterance->GetEngineId();
  if (!event_router->ExtensionHasEventListener(engine_id,
                                               tts_engine_events::kOnSpeak)) {
    // The extension removed its event listener after we processed the speak
    // call matching its voice.
    return;
  }

  auto event = std::make_unique<extensions::Event>(
      extensions::events::TTS_ENGINE_ON_SPEAK, tts_engine_events::kOnSpeak,
      std::move(args), profile);
  event_router->DispatchEventToExtension(engine_id, std::move(event));
}

void TtsExtensionEngine::Stop(content::TtsUtterance* utterance) {
  Stop(utterance->GetBrowserContext(), utterance->GetEngineId());
}

void TtsExtensionEngine::Stop(content::BrowserContext* browser_context,
                              const std::string& engine_id) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto event = std::make_unique<extensions::Event>(
      extensions::events::TTS_ENGINE_ON_STOP, tts_engine_events::kOnStop,
      base::Value::List(), profile);
  EventRouter::Get(profile)->DispatchEventToExtension(engine_id,
                                                      std::move(event));
}

void TtsExtensionEngine::Pause(content::TtsUtterance* utterance) {
  Pause(utterance->GetBrowserContext(), utterance->GetEngineId());
}

void TtsExtensionEngine::Pause(content::BrowserContext* browser_context,
                               const std::string& engine_id) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto event = std::make_unique<extensions::Event>(
      extensions::events::TTS_ENGINE_ON_PAUSE, tts_engine_events::kOnPause,
      base::Value::List(), profile);
  EventRouter* event_router = EventRouter::Get(profile);
  event_router->DispatchEventToExtension(engine_id, std::move(event));
  WarnIfMissingPauseOrResumeListener(profile, event_router, engine_id);
}

void TtsExtensionEngine::Resume(content::TtsUtterance* utterance) {
  Resume(utterance->GetBrowserContext(), utterance->GetEngineId());
}

void TtsExtensionEngine::Resume(content::BrowserContext* browser_context,
                                const std::string& engine_id) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto event = std::make_unique<extensions::Event>(
      extensions::events::TTS_ENGINE_ON_RESUME, tts_engine_events::kOnResume,
      base::Value::List(), profile);
  EventRouter* event_router = EventRouter::Get(profile);
  event_router->DispatchEventToExtension(engine_id, std::move(event));
  WarnIfMissingPauseOrResumeListener(profile, event_router, engine_id);
}

void TtsExtensionEngine::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {
  // No built-in extension engines on non-Chrome OS.
}

bool TtsExtensionEngine::IsBuiltInTtsEngineInitialized(
    content::BrowserContext* browser_context) {
  // Vacuously; no built in engines on other platforms yet. TODO: network tts?
  return true;
}

base::Value::List TtsExtensionEngine::BuildSpeakArgs(
    content::TtsUtterance* utterance,
    const content::VoiceData& voice) {
  // See if the engine supports the "end" event; if so, we can keep the
  // utterance around and track it. If not, we're finished with this
  // utterance now.
  bool sends_end_event =
      voice.events.find(content::TTS_EVENT_END) != voice.events.end();

  base::Value::List args;
  args.Append(utterance->GetText());

  // Pass through most options to the speech engine, but remove some
  // that are handled internally.
  base::Value::Dict options = utterance->GetOptions()->Clone();
  options.Remove(constants::kRequiredEventTypesKey);
  options.Remove(constants::kDesiredEventTypesKey);
  if (sends_end_event)
    options.Remove(constants::kEnqueueKey);
  options.Remove(constants::kSrcIdKey);
  options.Remove(constants::kIsFinalEventKey);
  options.Remove(constants::kOnEventKey);

  // Get the volume, pitch, and rate, but only if they weren't already in
  // the options. TODO(dmazzoni): these shouldn't be redundant.
  // http://crbug.com/463264
  if (!options.Find(constants::kRateKey)) {
    options.Set(constants::kRateKey, utterance->GetContinuousParameters().rate);
  }
  if (!options.Find(constants::kPitchKey)) {
    options.Set(constants::kPitchKey,
                utterance->GetContinuousParameters().pitch);
  }
  if (!options.Find(constants::kVolumeKey)) {
    options.Set(constants::kVolumeKey,
                utterance->GetContinuousParameters().volume);
  }

  // Add the voice name and language to the options if they're not
  // already there, since they might have been picked by the TTS controller
  // rather than directly by the client that requested the speech.
  if (!options.Find(constants::kVoiceNameKey))
    options.Set(constants::kVoiceNameKey, voice.name);
  if (!options.Find(constants::kLangKey))
    options.Set(constants::kLangKey, voice.lang);

  args.Append(std::move(options));
  args.Append(utterance->GetId());
  return args;
}

ExtensionFunction::ResponseAction
ExtensionTtsEngineUpdateVoicesFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_list());
  const base::Value& voices_data = args()[0];

  // Validate the voices and return an error if there's a problem.
  const char* error = nullptr;
  auto tts_voices = ValidateAndConvertToTtsVoiceVector(
      extension(), voices_data.GetList(),
      /* return_after_first_error = */ true, &error);
  if (error)
    return RespondNow(Error(error));

  // Save these voices to the extension's prefs if they validated.
  auto* extension_prefs = extensions::ExtensionPrefs::Get(browser_context());
  extension_prefs->UpdateExtensionPref(extension()->id(), kPrefTtsVoices,
                                       voices_data.Clone());

  // Notify that voices have changed.
  content::TtsController::GetInstance()->VoicesChanged();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionTtsEngineSendTtsEventFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);

  const auto& utterance_id_value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(utterance_id_value.is_int());
  int utterance_id = utterance_id_value.GetInt();

  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());
  const base::Value::Dict& event = args()[1].GetDict();

  const std::string* event_type = event.FindString(constants::kEventTypeKey);
  EXTENSION_FUNCTION_VALIDATE(event_type);

  int char_index = 0;
  const base::Value* char_index_value = event.Find(constants::kCharIndexKey);
  if (char_index_value) {
    EXTENSION_FUNCTION_VALIDATE(char_index_value->is_int());
    char_index = char_index_value->GetInt();
  }

  int length = -1;
  const base::Value* length_value = event.Find(constants::kLengthKey);
  if (length_value) {
    EXTENSION_FUNCTION_VALIDATE(length_value->is_int());
    length = length_value->GetInt();
  }

  // Make sure the extension has included this event type in its manifest.
  bool event_type_allowed = false;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  auto tts_voices = GetVoicesInternal(profile, extension());
  if (!tts_voices)
    return RespondNow(Error(constants::kErrorUndeclaredEventType));

  for (size_t i = 0; i < tts_voices->size(); i++) {
    const extensions::TtsVoice& voice = tts_voices->at(i);
    if (voice.event_types.find(*event_type) != voice.event_types.end()) {
      event_type_allowed = true;
      break;
    }
  }

  std::string error_message;
  if (*event_type == constants::kEventTypeError) {
    const std::string* err_msg = event.FindString(constants::kErrorMessageKey);
    error_message = err_msg != nullptr ? *err_msg : "";
  }

  if (!event_type_allowed)
    return RespondNow(Error(constants::kErrorUndeclaredEventType));

  content::TtsEventType tts_event_type;
  if (!GetTtsEventType(*event_type, &tts_event_type)) {
    EXTENSION_FUNCTION_VALIDATE(false);
  } else {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(crbug.com/40259646): Remove the workaround for enable lacros tts
    // support for testing and call
    // tts_crosapi_util::ShouldEnableLacrosTtsSupport() instead.
    if (content::TtsPlatform::GetInstance()->PlatformImplSupported()) {
      TtsClientLacros::GetForBrowserContext(browser_context())
          ->OnLacrosSpeechEngineTtsEvent(utterance_id, tts_event_type,
                                         char_index, length, error_message);
      return RespondNow(NoArguments());
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    // If lacros_tts_support is not enabled, TTS events routes to TtsController.
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id, tts_event_type, char_index, length, error_message);
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionTtsEngineSendTtsAudioFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);

  const auto& utterance_id_value = args()[0];
  EXTENSION_FUNCTION_VALIDATE(utterance_id_value.is_int());
  int utterance_id = utterance_id_value.GetInt();

  const base::Value::Dict* audio = args()[1].GetIfDict();
  EXTENSION_FUNCTION_VALIDATE(audio);

  const std::vector<uint8_t>* audio_buffer_blob =
      audio->FindBlob(tts_extension_api_constants::kAudioBufferKey);
  if (!audio_buffer_blob)
    return RespondNow(Error("No audio buffer found."));

  if (audio_buffer_blob->size() % 4 != 0)
    return RespondNow(Error("Invalid audio buffer format."));

  // Interpret the audio buffer as a sequence of float samples.
  size_t sample_count = audio_buffer_blob->size() / 4;
  std::vector<float> audio_buffer(sample_count);
  const float* view = reinterpret_cast<const float*>(&(*audio_buffer_blob)[0]);
  for (size_t i = 0; i < sample_count; i++, view++)
    audio_buffer[i] = *view;

  int char_index = 0;
  const base::Value* char_index_value =
      audio->Find(tts_extension_api_constants::kCharIndexKey);
  EXTENSION_FUNCTION_VALIDATE(char_index_value);
  EXTENSION_FUNCTION_VALIDATE(char_index_value->is_int());
  char_index = char_index_value->GetInt();

  std::optional<bool> is_last_buffer =
      audio->FindBool(tts_extension_api_constants::kIsLastBufferKey);
  EXTENSION_FUNCTION_VALIDATE(is_last_buffer);

  TtsExtensionEngine::GetInstance()->SendAudioBuffer(
      utterance_id, audio_buffer, char_index, *is_last_buffer);
  return RespondNow(NoArguments());
#else
  // Given tts engine json api definition, we should never get here.
  NOTREACHED_IN_MIGRATION();
  return RespondNow(Error("Unsupported on this platform."));
#endif
}
