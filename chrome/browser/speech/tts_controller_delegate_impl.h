// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_CONTROLLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SPEECH_TTS_CONTROLLER_DELEGATE_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/tts_controller_delegate.h"
#include "content/public/browser/tts_platform.h"
#include "url/gurl.h"

// Singleton class that manages Chrome side logic for TTS and TTS engine
// extension APIs.
class TtsControllerDelegateImpl : public content::TtsControllerDelegate {
 public:
  // Get the single instance of this class.
  static TtsControllerDelegateImpl* GetInstance();

  // TtsControllerDelegate overrides.
  int GetMatchingVoice(content::TtsUtterance* utterance,
                       std::vector<content::VoiceData>& voices) override;
  void UpdateUtteranceDefaultsFromPrefs(content::TtsUtterance* utterance,
                                        double* rate,
                                        double* pitch,
                                        double* volume) override;
  void SetTtsEngineDelegate(content::TtsEngineDelegate* delegate) override;
  content::TtsEngineDelegate* GetTtsEngineDelegate() override;

 protected:
  TtsControllerDelegateImpl();
  ~TtsControllerDelegateImpl() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest, TestTtsControllerShutdown);
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest, TestGetMatchingVoice);
  FRIEND_TEST_ALL_PREFIXES(TtsControllerTest,
                           TestTtsControllerUtteranceDefaults);

  virtual const PrefService* GetPrefService(content::TtsUtterance* utterance);

  friend struct base::DefaultSingletonTraits<TtsControllerDelegateImpl>;

  // The delegate that processes TTS requests with user-installed extensions.
  content::TtsEngineDelegate* tts_engine_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TtsControllerDelegateImpl);
};

#endif  // CHROME_BROWSER_SPEECH_TTS_CONTROLLER_DELEGATE_IMPL_H_
