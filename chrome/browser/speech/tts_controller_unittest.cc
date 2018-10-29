// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/speech/tts_controller_impl.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_speech_synthesis_constants.h"

class TtsControllerTest : public testing::Test {
};

// Platform Tts implementation that does nothing.
class DummyTtsPlatformImpl : public TtsPlatformImpl {
 public:
  DummyTtsPlatformImpl() {}
  ~DummyTtsPlatformImpl() override {}
  bool PlatformImplAvailable() override { return true; }
  bool Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params) override {
    return true;
  }
  bool IsSpeaking() override { return false; }
  bool StopSpeaking() override { return true; }
  void Pause() override {}
  void Resume() override {}
  void GetVoices(std::vector<VoiceData>* out_voices) override {}
  std::string error() override { return std::string(); }
  void clear_error() override {}
  void set_error(const std::string& error) override {}
};

// Subclass of TtsController with a public ctor and dtor.
class TestableTtsController : public TtsControllerImpl {
 public:
  TestableTtsController() {}
  ~TestableTtsController() override {}

  PrefService* pref_service_ = nullptr;

 private:
  const PrefService* GetPrefService(const Utterance* utterance) override {
    return pref_service_;
  }
};

TEST_F(TtsControllerTest, TestTtsControllerShutdown) {
  DummyTtsPlatformImpl platform_impl;
  TestableTtsController* controller =
      new TestableTtsController();

  controller->SetPlatformImpl(&platform_impl);

  Utterance* utterance1 = new Utterance(nullptr);
  utterance1->set_can_enqueue(true);
  utterance1->set_src_id(1);
  controller->SpeakOrEnqueue(utterance1);

  Utterance* utterance2 = new Utterance(nullptr);
  utterance2->set_can_enqueue(true);
  utterance2->set_src_id(2);
  controller->SpeakOrEnqueue(utterance2);

  // Make sure that deleting the controller when there are pending
  // utterances doesn't cause a crash.
  delete controller;
}

TEST_F(TtsControllerTest, TestGetMatchingVoice) {
  std::unique_ptr<TestableTtsController> tts_controller =
      std::make_unique<TestableTtsController>();
#if defined(OS_CHROMEOS)
  TestingPrefServiceSimple pref_service_;
  // Uses default pref voices.
  std::unique_ptr<base::DictionaryValue> lang_to_voices =
      std::make_unique<base::DictionaryValue>();
  lang_to_voices->SetKey(
      "es", base::Value("{\"name\":\"Voice7\",\"extension\":\"id7\"}"));
  lang_to_voices->SetKey(
      "noLanguage", base::Value("{\"name\":\"Android\",\"extension\":\"\"}"));
  pref_service_.registry()->RegisterDictionaryPref(
      prefs::kTextToSpeechLangToVoiceName, std::move(lang_to_voices));
  tts_controller->pref_service_ = &pref_service_;
#endif  // defined(OS_CHROMEOS)

  {
    // Calling GetMatchingVoice with no voices returns -1.
    Utterance utterance(nullptr);
    std::vector<VoiceData> voices;
    EXPECT_EQ(-1, tts_controller->GetMatchingVoice(&utterance, voices));
  }

  {
    // Calling GetMatchingVoice with any voices returns the first one
    // even if there are no criteria that match.
    Utterance utterance(nullptr);
    std::vector<VoiceData> voices;
    voices.push_back(VoiceData());
    voices.push_back(VoiceData());
    EXPECT_EQ(0, tts_controller->GetMatchingVoice(&utterance, voices));
  }

  {
    // If nothing else matches, the English voice is returned.
    // (In tests the language will always be English.)
    Utterance utterance(nullptr);
    std::vector<VoiceData> voices;
    VoiceData fr_voice;
    fr_voice.lang = "fr";
    voices.push_back(fr_voice);
    VoiceData en_voice;
    en_voice.lang = "en";
    voices.push_back(en_voice);
    VoiceData de_voice;
    de_voice.lang = "de";
    voices.push_back(de_voice);
    EXPECT_EQ(1, tts_controller->GetMatchingVoice(&utterance, voices));
  }

  {
    // Check precedence of various matching criteria.
    std::vector<VoiceData> voices;
    VoiceData voice0;
    voices.push_back(voice0);
    VoiceData voice1;
    voice1.events.insert(TTS_EVENT_WORD);
    voices.push_back(voice1);
    VoiceData voice2;
    voice2.lang = "de-DE";
    voices.push_back(voice2);
    VoiceData voice3;
    voice3.lang = "fr-CA";
    voices.push_back(voice3);
    VoiceData voice4;
    voice4.name = "Voice4";
    voices.push_back(voice4);
    VoiceData voice5;
    voice5.extension_id = "id5";
    voices.push_back(voice5);
    VoiceData voice6;
    voice6.extension_id = "id7";
    voice6.name = "Voice6";
    voice6.lang = "es-es";
    voices.push_back(voice6);
    VoiceData voice7;
    voice7.extension_id = "id7";
    voice7.name = "Voice7";
    voice7.lang = "es-mx";
    voices.push_back(voice7);
    VoiceData voice8;
    voice8.extension_id = "";
    voice8.name = "Android";
    voice8.lang = "";
    voice8.native = true;
    voices.push_back(voice8);

    Utterance utterance(nullptr);
    EXPECT_EQ(0, tts_controller->GetMatchingVoice(&utterance, voices));

    std::set<TtsEventType> types;
    types.insert(TTS_EVENT_WORD);
    utterance.set_required_event_types(types);
    EXPECT_EQ(1, tts_controller->GetMatchingVoice(&utterance, voices));

    utterance.set_lang("de-DE");
    EXPECT_EQ(2, tts_controller->GetMatchingVoice(&utterance, voices));

    utterance.set_lang("fr-FR");
    EXPECT_EQ(3, tts_controller->GetMatchingVoice(&utterance, voices));

    utterance.set_voice_name("Voice4");
    EXPECT_EQ(4, tts_controller->GetMatchingVoice(&utterance, voices));

    utterance.set_voice_name("");
    utterance.set_extension_id("id5");
    EXPECT_EQ(5, tts_controller->GetMatchingVoice(&utterance, voices));

#if defined(OS_CHROMEOS)
    // Voice6 is matched when the utterance locale exactly matches its locale.
    utterance.set_extension_id("");
    utterance.set_lang("es-es");
    EXPECT_EQ(6, tts_controller->GetMatchingVoice(&utterance, voices));

    // The 7th voice is the default for "es", even though the utterance is
    // "es-ar". |voice6| is not matched because it is not the default.
    utterance.set_extension_id("");
    utterance.set_lang("es-ar");
    EXPECT_EQ(7, tts_controller->GetMatchingVoice(&utterance, voices));

    // The 8th voice is like the built-in "Android" voice, it has no lang
    // and no extension ID. Make sure it can still be matched.
    utterance.set_voice_name("Android");
    utterance.set_extension_id("");
    utterance.set_lang("");
    EXPECT_EQ(8, tts_controller->GetMatchingVoice(&utterance, voices));
#endif  // defined(OS_CHROMEOS)
  }

  {
    // Check voices against system language.
    std::vector<VoiceData> voices;
    VoiceData voice0;
    voice0.extension_id = "id0";
    voice0.name = "voice0";
    voice0.lang = "en-GB";
    voices.push_back(voice0);
    VoiceData voice1;
    voice1.extension_id = "id1";
    voice1.name = "voice1";
    voice1.lang = "en-US";
    voices.push_back(voice1);
    Utterance utterance(nullptr);

    // voice1 is matched against the exact default system language.
    g_browser_process->SetApplicationLocale("en-US");
    utterance.set_lang("");
    EXPECT_EQ(1, tts_controller->GetMatchingVoice(&utterance, voices));

    // voice0 is matched against the system language which has no region piece.
    g_browser_process->SetApplicationLocale("en");
    utterance.set_lang("");
    EXPECT_EQ(0, tts_controller->GetMatchingVoice(&utterance, voices));

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
    EXPECT_EQ(0, tts_controller->GetMatchingVoice(&utterance, voices));
#endif  // defined(OS_CHROMEOS)
  }
}

#if defined(OS_CHROMEOS)
TEST_F(TtsControllerTest, TestTtsControllerUtteranceDefaults) {
  std::unique_ptr<TestableTtsController> controller =
      std::make_unique<TestableTtsController>();

  std::unique_ptr<Utterance> utterance1 = std::make_unique<Utterance>(nullptr);
  // Initialized to default (unset constant) values.
  EXPECT_EQ(blink::kWebSpeechSynthesisDoublePrefNotSet,
            utterance1->continuous_parameters().rate);
  EXPECT_EQ(blink::kWebSpeechSynthesisDoublePrefNotSet,
            utterance1->continuous_parameters().pitch);
  EXPECT_EQ(blink::kWebSpeechSynthesisDoublePrefNotSet,
            utterance1->continuous_parameters().volume);

  controller->UpdateUtteranceDefaults(utterance1.get());
  // Updated to global defaults.
  EXPECT_EQ(blink::kWebSpeechSynthesisDefaultTextToSpeechRate,
            utterance1->continuous_parameters().rate);
  EXPECT_EQ(blink::kWebSpeechSynthesisDefaultTextToSpeechPitch,
            utterance1->continuous_parameters().pitch);
  EXPECT_EQ(blink::kWebSpeechSynthesisDefaultTextToSpeechVolume,
            utterance1->continuous_parameters().volume);

  // Now we will set prefs and expect those to be used as defaults.
  TestingPrefServiceSimple pref_service_;
  pref_service_.registry()->RegisterDoublePref(prefs::kTextToSpeechRate, 1.5);
  pref_service_.registry()->RegisterDoublePref(prefs::kTextToSpeechPitch, 2.0);
  pref_service_.registry()->RegisterDoublePref(prefs::kTextToSpeechVolume, 0.5);
  controller->pref_service_ = &pref_service_;

  std::unique_ptr<Utterance> utterance2 = std::make_unique<Utterance>(nullptr);
  controller->UpdateUtteranceDefaults(utterance2.get());
  // Updated to pref values.
  EXPECT_EQ(1.5f, utterance2->continuous_parameters().rate);
  EXPECT_EQ(2.0f, utterance2->continuous_parameters().pitch);
  EXPECT_EQ(0.5f, utterance2->continuous_parameters().volume);
}
#endif  // defined(OS_CHROMEOS)
