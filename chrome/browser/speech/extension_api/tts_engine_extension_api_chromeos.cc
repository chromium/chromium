// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_api_chromeos.h"

#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

TtsExtensionEngine* TtsExtensionEngine::GetInstance() {
  static base::NoDestructor<TtsExtensionEngineChromeOS> tts_extension_engine;
  return tts_extension_engine.get();
}

void TtsExtensionEngineChromeOS::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {
  if (disable_built_in_tts_engine_for_testing_)
    return;

  Profile* profile = Profile::FromBrowserContext(browser_context);

  // Load the component extensions into this profile.
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(extension_service);
  extension_service->component_loader()->AddChromeOsSpeechSynthesisExtensions();
}

bool TtsExtensionEngineChromeOS::IsBuiltInTtsEngineInitialized(
    content::BrowserContext* browser_context) {
  if (!browser_context || disable_built_in_tts_engine_for_testing_)
    return true;

  std::vector<content::VoiceData> voices;
  GetVoices(browser_context, &voices);
  bool saw_google_tts = false;
  bool saw_espeak = false;
  for (const auto& voice : voices) {
    saw_google_tts |=
        voice.engine_id == extension_misc::kGoogleSpeechSynthesisExtensionId;
    saw_espeak |=
        voice.engine_id == extension_misc::kEspeakSpeechSynthesisExtensionId;
  }

  // When running on a real Chrome OS environment, require both Google tts and
  // Espeak to be initialized; otherwise, only check for Espeak (i.e. on a
  // non-Chrome OS linux system running the CHrome OS variant of Chrome).
  return base::SysInfo::IsRunningOnChromeOS() ? (saw_google_tts && saw_espeak)
                                              : saw_espeak;
}
