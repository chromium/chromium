// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for the TTS Controller.

#include "chrome/browser/speech/tts_controller_delegate_impl.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"

// Subclass of TtsController with a public ctor and dtor.
class MockTtsControllerDelegate : public TtsControllerDelegateImpl {
 public:
  MockTtsControllerDelegate() = default;
  ~MockTtsControllerDelegate() override = default;

  raw_ptr<PrefService> pref_service_ = nullptr;

 private:
  const PrefService* GetPrefService(content::TtsUtterance* utterance) override {
    return pref_service_;
  }
};

TEST(TtsControllerDelegateImplTest, TestTtsControllerUtteranceDefaults) {
  std::unique_ptr<MockTtsControllerDelegate> tts_controller_delegate =
      std::make_unique<MockTtsControllerDelegate>();

  double rate = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  double pitch = blink::mojom::kSpeechSynthesisDoublePrefNotSet;
  double volume = blink::mojom::kSpeechSynthesisDoublePrefNotSet;

  std::unique_ptr<content::TtsUtterance> utterance1 =
      content::TtsUtterance::Create();
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
      content::TtsUtterance::Create();
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
      content::TtsUtterance::Create();
  tts_controller_delegate->UpdateUtteranceDefaultsFromPrefs(
      utterance3.get(), &rate, &pitch, &volume);
  // Values should not change.
  EXPECT_EQ(1.1f, rate);
  EXPECT_EQ(1.2f, pitch);
  EXPECT_EQ(1.3f, volume);

  // If we explicitly set rate, pitch and volume to the default values, they
  // should not be changed.
  rate = blink::mojom::kSpeechSynthesisDefaultRate;
  pitch = blink::mojom::kSpeechSynthesisDefaultPitch;
  volume = blink::mojom::kSpeechSynthesisDefaultVolume;

  std::unique_ptr<content::TtsUtterance> utterance4 =
      content::TtsUtterance::Create();
  tts_controller_delegate->UpdateUtteranceDefaultsFromPrefs(
      utterance4.get(), &rate, &pitch, &volume);
  // Values should not change.
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultRate, rate);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultPitch, pitch);
  EXPECT_EQ(blink::mojom::kSpeechSynthesisDefaultVolume, volume);
}

TEST(TtsControllerDelegateImplTest, GetPreferredVoiceIdsForUtterance) {
  MockTtsControllerDelegate delegate;
  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create();
  auto ids = delegate.GetPreferredVoiceIdsForUtterance(utterance.get());
  EXPECT_EQ(nullptr, ids.get());

  TestingPrefServiceSimple pref_service;
  // Uses default pref voices.
  auto lang_to_voices =
      base::Value::Dict()
          .Set("es", "{\"name\":\"Voice7\",\"extension\":\"id7\"}")
          .Set("he", "{\"name\":\"Voice8\",\"extension\":\"id8\"}")
          .Set("noLanguageCode", "{\"name\":\"Android\",\"extension\":\"x\"}");
  pref_service.registry()->RegisterDictionaryPref(
      prefs::kTextToSpeechLangToVoiceName, std::move(lang_to_voices));
  delegate.pref_service_ = &pref_service;

  ids = delegate.GetPreferredVoiceIdsForUtterance(utterance.get());
  ASSERT_TRUE(ids.get());
  EXPECT_FALSE(ids->lang_voice_id.has_value());
  EXPECT_FALSE(ids->locale_voice_id.has_value());
  ASSERT_TRUE(ids->any_locale_voice_id.has_value());
  EXPECT_EQ("Android", ids->any_locale_voice_id->name);
  EXPECT_EQ("x", ids->any_locale_voice_id->id);

  // Change the language of the Utterance to 'es' which should match one of the
  // registered keys.
  utterance->SetLang("es");
  ids = delegate.GetPreferredVoiceIdsForUtterance(utterance.get());
  ASSERT_TRUE(ids.get());
  ASSERT_TRUE(ids->lang_voice_id.has_value());
  EXPECT_EQ("Voice7", ids->lang_voice_id->name);
  EXPECT_EQ("id7", ids->lang_voice_id->id);
  EXPECT_FALSE(ids->locale_voice_id.has_value());
  ASSERT_TRUE(ids->any_locale_voice_id.has_value());
  EXPECT_EQ("Android", ids->any_locale_voice_id->name);
  EXPECT_EQ("x", ids->any_locale_voice_id->id);

  // Change the application locale to 'he' which should match one of the
  // registered keys.
  g_browser_process->SetApplicationLocale("he");
  ids = delegate.GetPreferredVoiceIdsForUtterance(utterance.get());
  ASSERT_TRUE(ids.get());
  ASSERT_TRUE(ids->lang_voice_id.has_value());
  EXPECT_EQ("Voice7", ids->lang_voice_id->name);
  EXPECT_EQ("id7", ids->lang_voice_id->id);
  EXPECT_TRUE(ids->locale_voice_id.has_value());
  EXPECT_EQ("Voice8", ids->locale_voice_id->name);
  EXPECT_EQ("id8", ids->locale_voice_id->id);
  ASSERT_TRUE(ids->any_locale_voice_id.has_value());
  EXPECT_EQ("Android", ids->any_locale_voice_id->name);
  EXPECT_EQ("x", ids->any_locale_voice_id->id);
}
