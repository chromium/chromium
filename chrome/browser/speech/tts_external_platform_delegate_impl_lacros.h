// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_EXTERNAL_PLATFORM_DELEGATE_IMPL_LACROS_H_
#define CHROME_BROWSER_SPEECH_TTS_EXTERNAL_PLATFORM_DELEGATE_IMPL_LACROS_H_

#include "base/no_destructor.h"
#include "content/public/browser/tts_platform.h"

class ExternalPlatformDelegateImplLacros
    : public content::ExternalPlatformDelegate {
 public:
  static ExternalPlatformDelegateImplLacros* GetInstance();
  ExternalPlatformDelegateImplLacros();
  ExternalPlatformDelegateImplLacros(
      const ExternalPlatformDelegateImplLacros&) = delete;
  ExternalPlatformDelegateImplLacros& operator=(
      const ExternalPlatformDelegateImplLacros&) = delete;
  ~ExternalPlatformDelegateImplLacros() override;

  // ExternalPlatformDelegateImplLacros:
  void GetVoicesForBrowserContext(
      content::BrowserContext* browser_context,
      const GURL& source_url,
      std::vector<content::VoiceData>* out_voices) override;
  void Enqueue(std::unique_ptr<content::TtsUtterance> utterance) override;
  void Stop(const GURL& source_url) override;
  void Pause() override;
  void Resume() override;

 private:
  friend class base::NoDestructor<ExternalPlatformDelegateImplLacros>;
};

#endif  // CHROME_BROWSER_SPEECH_TTS_EXTERNAL_PLATFORM_DELEGATE_IMPL_LACROS_H_
