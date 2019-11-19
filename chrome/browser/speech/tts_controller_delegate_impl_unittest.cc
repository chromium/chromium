// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "chrome/browser/speech/tts_controller_delegate_impl.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"

class TtsControllerTest : public testing::Test {};

// Subclass of TtsController with a public ctor and dtor.
class MockTtsControllerDelegate : public TtsControllerDelegateImpl {
 public:
  MockTtsControllerDelegate() {}
  ~MockTtsControllerDelegate() override {}

  PrefService* pref_service_ = nullptr;

 private:
  const PrefService* GetPrefService(content::TtsUtterance* utterance) override {
    return pref_service_;
  }
};

TEST_F(TtsControllerTest, TestGetMatchingVoice) {
  std::unique_ptr<MockTtsControllerDelegate> tts_controller_delegate =
      std::make_unique<MockTtsControllerDelegate>();
#if defined(OS_CHROMEOS)
  TestingPrefServiceSimple pref_service_;
  // Uses default pref voices.
  base::Value lang_to_voices(base::Value::Type::DICTIONARY);
  lang_to_voices.SetKey(
      "es", base::Value("{\"name\":\"Voice7\",\"extension\":\"id7\"}"));
  lang_to_voices.SetKey(
      "noLanguage", base::Value("{\"name\":\"Android\",\"extension\":\"\"}"));
  pref_service_.registry()->RegisterDictionaryPref(
      prefs::kTextToSpeechLangToVoiceName, std::move(lang_to_voices));
  tts_controller_delegate->pref_service_ = &pref_service_;
#endif  // defined(OS_CHROMEOS)

  {
    // Calling GetMatchingVoice with no voices returns -1.
    std::unique_ptr<content::TtsUtterance> utterance(
        content::TtsUtterance::Create(nullptr));
    std::vector<content::VoiceData> voices;
    EXPECT_EQ(
        -1, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // Calling GetMatchingVoice with any voices returns the first one
    // even if there are no criteria that match.
    std::unique_ptr<content::TtsUtterance> utterance(
        content::TtsUtterance::Create(nullptr));
    std::vector<content::VoiceData> voices;
    voices.push_back(content::VoiceData());
    voices.push_back(content::VoiceData());
    EXPECT_EQ(
        0, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // If nothing else matches, the English voice is returned.
    // (In tests the language will always be English.)
    std::unique_ptr<content::TtsUtterance> utterance(
        content::TtsUtterance::Create(nullptr));
    std::vector<content::VoiceData> voices;
    content::VoiceData fr_voice;
    fr_voice.lang = "fr";
    voices.push_back(fr_voice);
    content::VoiceData en_voice;
    en_voice.lang = "en";
    voices.push_back(en_voice);
    content::VoiceData de_voice;
    de_voice.lang = "de";
    voices.push_back(de_voice);
    EXPECT_EQ(
        1, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));
  }

  {
    // Check precedence of various matching criteria.
    std::vector<content::VoiceData> voices;
    content::VoiceData voice0;
    voices.push_back(voice0);
    content::VoiceData voice1;
    voice1.events.insert(content::TTS_EVENT_WORD);
    voices.push_back(voice1);
    content::VoiceData voice2;
    voice2.lang = "de-DE";
    voices.push_back(voice2);
    content::VoiceData voice3;
    voice3.lang = "fr-CA";
    voices.push_back(voice3);
    content::VoiceData voice4;
    voice4.name = "Voice4";
    voices.push_back(voice4);
    content::VoiceData voice5;
    voice5.engine_id = "id5";
    voices.push_back(voice5);
    content::VoiceData voice6;
    voice6.engine_id = "id7";
    voice6.name = "Voice6";
    voice6.lang = "es-es";
    voices.push_back(voice6);
    content::VoiceData voice7;
    voice7.engine_id = "id7";
    voice7.name = "Voice7";
    voice7.lang = "es-mx";
    voices.push_back(voice7);
    content::VoiceData voice8;
    voice8.engine_id = "";
    voice8.name = "Android";
    voice8.lang = "";
    voice8.native = true;
    voices.push_back(voice8);

    std::unique_ptr<content::TtsUtterance> utterance(
        content::TtsUtterance::Create(nullptr));
    EXPECT_EQ(
        0, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

    std::set<content::TtsEventType> types;
    types.insert(content::TTS_EVENT_WORD);
    utterance->SetRequiredEventTypes(types);
    EXPECT_EQ(
        1, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

    utterance->SetLang("de-DE");
    EXPECT_EQ(
        2, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

    utterance->SetLang("fr-FR");
    EXPECT_EQ(
        3, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

    utterance->SetVoiceName("Voice4");
    EXPECT_EQ(
        4, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

    utterance->SetVoiceName("");
    utterance->SetEngineId("id5");
    EXPECT_EQ(
        5, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

#if defined(OS_CHROMEOS)
    // Voice6 is matched when the utterance locale exactly matches its locale.
    utterance->SetEngineId("");
    utterance->SetLang("es-es");
    EXPECT_EQ(
        6, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

    // The 7th voice is the default for "es", even though the utterance is
    // "es-ar". |voice6| is not matched because it is not the default.
    utterance->SetEngineId("");
    utterance->SetLang("es-ar");
    EXPECT_EQ(
        7, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

    // The 8th voice is like the built-in "Android" voice, it has no lang
    // and no extension ID. Make sure it can still be matched.
    utterance->SetVoiceName("Android");
    utterance->SetEngineId("");
    utterance->SetLang("");
    EXPECT_EQ(
        8, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));
#endif  // defined(OS_CHROMEOS)
  }

  {
    // Check voices against system language.
    std::vector<content::VoiceData> voices;
    content::VoiceData voice0;
    voice0.engine_id = "id0";
    voice0.name = "voice0";
    voice0.lang = "en-GB";
    voices.push_back(voice0);
    content::VoiceData voice1;
    voice1.engine_id = "id1";
    voice1.name = "voice1";
    voice1.lang = "en-US";
    voices.push_back(voice1);
    std::unique_ptr<content::TtsUtterance> utterance(
        content::TtsUtterance::Create(nullptr));

    // voice1 is matched against the exact default system language.
    g_browser_process->SetApplicationLocale("en-US");
    utterance->SetLang("");
    EXPECT_EQ(
        1, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

    // voice0 is matched against the system language which has no region piece.
    g_browser_process->SetApplicationLocale("en");
    utterance->SetLang("");
    EXPECT_EQ(
        0, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));

#if defined(OS_CHROMEOS)
    // Add another preference.
    std::unique_ptr<base::DictionaryValue> lang_to_voices =
        std::make_unique<base::DictionaryValue>();
    lang_to_voices->SetKey(
        "en", base::Value("{\"name\":\"voice0\",\"extension\":\"id0\"}"));

    pref_service_.SetUserPref(prefs::kTextToSpeechLangToVoiceName,
                              std::move(lang_to_voices));

    // voice0 is matched against the pref over the system language.
    g_browser_process->SetApplicationLocale("en-US");
    EXPECT_EQ(
        0, tts_controller_delegate->GetMatchingVoice(utterance.get(), voices));
#endif  // defined(OS_CHROMEOS)
  }
}

#if defined(OS_CHROMEOS)
TEST_F(TtsControllerTest, TestTtsControllerUtteranceDefaults) {
  std::unique_ptr<MockTtsControllerDelegate> tts_controller_delegate =
      std::make_unique<MockTtsControllerDelegate>();

  double rate = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  double pitch = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  double volume = blink::mojom::kSpeechSynthesisDoublePrefNotSet;

  std::unique_ptr<content::TtsUtterance> utterance1 =
      content::TtsUtterance::Create(nullptr);
  tts_controller_delegate->UpdateUtteranceDefaultsFromPrefs(
      utterance1.get(), &rate, &pitch, &volume);
  // Updated to global defaults.
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultRate, rate);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultPitch, pitch);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultVolume, volume);

  // Now we will set prefs and expect those to be used as defaults.
  rate = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  pitch = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  volume = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  TestingPrefServiceSimple pref_service_;
  pref_service_.registry()->RegisterDoublePref(prefs::kTextToSpeechRate, 1.5);
  pref_service_.registry()->RegisterDoublePref(prefs::kTextToSpeechPitch, 2.0);
  pref_service_.registry()->RegisterDoublePref(prefs::kTextToSpeechVolume, 0.5);
  tts_controller_delegate->pref_service_ = &pref_service_;

  std::unique_ptr<content::TtsUtterance> utterance2 =
      content::TtsUtterance::Create(nullptr);
  tts_controller_delegate->UpdateUtteranceDefaultsFromPrefs(
      utterance2.get(), &rate, &pitch, &volume);
  // Updated to pref values.
  EXPECT_EQ(1.5f, rate);
  EXPECT_EQ(2.0f, pitch);
  EXPECT_EQ(0.5f, volume);

  // If we explicitly set rate, pitch and volume, they should not be changed.
  rate = 1.1f;
  pitch = 1.2f;
  volume = 1.3f;

  std::unique_ptr<content::TtsUtterance> utterance3 =
      content::TtsUtterance::Create(nullptr);
  tts_controller_delegate->UpdateUtteranceDefaultsFromPrefs(
      utterance3.get(), &rate, &pitch, &volume);
  // Updated to pref values.
  EXPECT_EQ(1.1f, rate);
  EXPECT_EQ(1.2f, pitch);
  EXPECT_EQ(1.3f, volume);
}
#endif  // defined(OS_CHROMEOS)
