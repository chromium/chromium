// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_CONTROLLER_DELEGATE_CHROMEOS_IMPL_H_
#define CHROME_BROWSER_SPEECH_TTS_CONTROLLER_DELEGATE_CHROMEOS_IMPL_H_

#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "content/public/browser/tts_controller_delegate_chromeos.h"

class PrefService;

// Singleton class that manages Chrome-side logic for TTS and TTS engine
// extension APIs on ChromeOS.
class TtsControllerDelegateChromeOSImpl
    : public content::TtsControllerDelegate {
 public:
  // Get the single instance of this class.
  static TtsControllerDelegateChromeOSImpl* GetInstance();

  TtsControllerDelegateChromeOSImpl(const TtsControllerDelegateChromeOSImpl&) =
      delete;
  TtsControllerDelegateChromeOSImpl& operator=(
      const TtsControllerDelegateChromeOSImpl&) = delete;

  // TtsControllerDelegate overrides.
  std::unique_ptr<content::TtsControllerDelegate::PreferredVoiceIds>
  GetPreferredVoiceIdsForUtterance(content::TtsUtterance* utterance) override;
  void UpdateUtteranceDefaultsFromPrefs(content::TtsUtterance* utterance,
                                        double* rate,
                                        double* pitch,
                                        double* volume) override;
  bool IsFallbackEngine(std::string_view engine_id) override;

 protected:
  TtsControllerDelegateChromeOSImpl();
  ~TtsControllerDelegateChromeOSImpl() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TtsControllerDelegateChromeOSImplTest,
                           TestTtsControllerUtteranceDefaults);

  virtual const PrefService* GetPrefService(content::TtsUtterance* utterance);

  const base::DictValue* GetLangToVoicePref(content::TtsUtterance* utterance);

  friend struct base::DefaultSingletonTraits<TtsControllerDelegateChromeOSImpl>;
};

// Aliases for compatibility.
using TtsControllerDelegateChromeOS = TtsControllerDelegateChromeOSImpl;
using TtsControllerDelegateImpl = TtsControllerDelegateChromeOSImpl;

#endif  // CHROME_BROWSER_SPEECH_TTS_CONTROLLER_DELEGATE_CHROMEOS_IMPL_H_
