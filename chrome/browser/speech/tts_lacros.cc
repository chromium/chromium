// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_lacros.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_client_lacros.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chrome/browser/speech/tts_external_platform_delegate_impl_lacros.h"
#include "content/public/browser/tts_utterance.h"

// static
TtsPlatformImplLacros* TtsPlatformImplLacros::GetInstance() {
  static base::NoDestructor<TtsPlatformImplLacros> tts_platform;
  return tts_platform.get();
}

TtsPlatformImplLacros::TtsPlatformImplLacros() {
  if (PlatformImplSupported()) {
    external_platform_delegate_ =
        ExternalPlatformDelegateImplLacros::GetInstance();
    profile_manager_observation_.Observe(g_browser_process->profile_manager());
  }
}

TtsPlatformImplLacros::~TtsPlatformImplLacros() = default;

void TtsPlatformImplLacros::OnProfileAdded(Profile* profile) {
  // Create TtsClientLacros for |profile|.
  TtsClientLacros::GetForBrowserContext(profile);
}

void TtsPlatformImplLacros::OnProfileManagerDestroying() {
  if (PlatformImplSupported())
    profile_manager_observation_.Reset();
}

bool TtsPlatformImplLacros::PlatformImplSupported() {
  return tts_crosapi_util::ShouldEnableLacrosTtsSupport();
}

bool TtsPlatformImplLacros::PlatformImplInitialized() {
  return true;
}

content::ExternalPlatformDelegate*
TtsPlatformImplLacros::GetExternalPlatformDelegate() {
  return external_platform_delegate_;
}

std::string TtsPlatformImplLacros::GetError() {
  return "";
}

bool TtsPlatformImplLacros::StopSpeaking() {
  return false;
}

bool TtsPlatformImplLacros::IsSpeaking() {
  return false;
}

void TtsPlatformImplLacros::FinalizeVoiceOrdering(
    std::vector<content::VoiceData>& voices) {}
