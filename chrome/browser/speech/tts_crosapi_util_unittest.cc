// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/speech/extension_api/tts_extension_api_constants.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kText[] = "hello, world";
const char kVoiceName[] = "Alice";
const char kLang[] = "en-GB";
const int kSrcId = 12345;
const char kSrcUrl[] = "http://test.com";
const char kEngineId[] = "test_engine_id";
const double kRate = 1.0f;
const double kPitch = 0.5f;
const double kVolume = 0.9f;
const base::UnguessableToken kBrowerContextId =
    base::UnguessableToken::Create();

bool EventTypesMatches(
    const std::set<content::TtsEventType>& tts_event_types_in,
    const std::set<content::TtsEventType>& tts_event_types_out) {
  if (tts_event_types_in.size() != tts_event_types_out.size())
    return false;

  for (const auto& event_type : tts_event_types_in) {
    if (tts_event_types_out.find(event_type) == tts_event_types_out.end())
      return false;
  }

  return true;
}

}  // namespace

class TtsUtteranceMojomTest : public testing::Test {
 public:
  TtsUtteranceMojomTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        rvh_test_enabler_(new content::RenderViewHostTestEnabler()) {}

  TtsUtteranceMojomTest(const TtsUtteranceMojomTest&) = delete;
  TtsUtteranceMojomTest& operator=(const TtsUtteranceMojomTest&) = delete;

  void SetUp() override {
    testing::Test::SetUp();

    testing_profile_ = TestingProfile::Builder().Build();
    web_contents_ = CreateTestWebContents();
  }

 protected:
  content::BrowserContext* browser_context() { return testing_profile_.get(); }
  content::WebContents* GetWebContents() { return web_contents_.get(); }

 private:
  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    auto site_instance = content::SiteInstance::Create(browser_context());
    return content::WebContentsTester::CreateTestWebContents(
        browser_context(), std::move(site_instance));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// Test that every field in content::TtsUtterance in correctly converted
// with a round trip conversion to and from crosapi::mojom::TtsUtterance.
TEST_F(TtsUtteranceMojomTest, RoundTripWithoutOptions) {
  std::unique_ptr<content::TtsUtterance> in_utterance =
      content::TtsUtterance::Create();
  in_utterance->SetText(kText);
  in_utterance->SetVoiceName(kVoiceName);
  in_utterance->SetLang(kLang);
  in_utterance->SetSrcId(kSrcId);
  in_utterance->SetSrcUrl(GURL(kSrcUrl));
  in_utterance->SetEngineId(kEngineId);
  in_utterance->SetContinuousParameters(kRate, kPitch, kVolume);
  in_utterance->SetShouldClearQueue(true);

  std::set<content::TtsEventType> required_event_types;
  required_event_types.insert(content::TTS_EVENT_START);
  required_event_types.insert(content::TTS_EVENT_WORD);
  required_event_types.insert(content::TTS_EVENT_END);
  in_utterance->SetRequiredEventTypes(required_event_types);

  std::set<content::TtsEventType> desired_event_types;
  desired_event_types.insert(content::TTS_EVENT_CANCELLED);
  desired_event_types.insert(content::TTS_EVENT_RESUME);
  desired_event_types.insert(content::TTS_EVENT_PAUSE);
  in_utterance->SetDesiredEventTypes(desired_event_types);

  // Round trip conversion to and from mojom utterance.
  auto mojo_utterance = tts_crosapi_util::ToMojo(in_utterance.get());
  mojo_utterance->browser_context_id = kBrowerContextId;

  {
    // Create TtsUtterance for a Lacros Utterance.
    std::unique_ptr<content::TtsUtterance> out_utterance =
        tts_crosapi_util::CreateUtteranceFromMojo(
            mojo_utterance, /*should_always_be_spoken=*/true);

    ASSERT_EQ(out_utterance->GetText(), kText);
    ASSERT_EQ(out_utterance->GetVoiceName(), kVoiceName);
    ASSERT_EQ(out_utterance->GetLang(), kLang);
    ASSERT_EQ(out_utterance->GetSrcId(), kSrcId);
    ASSERT_EQ(out_utterance->GetSrcUrl(), GURL(kSrcUrl));
    ASSERT_EQ(out_utterance->GetEngineId(), kEngineId);
    auto continuouse_params = out_utterance->GetContinuousParameters();
    ASSERT_EQ(continuouse_params.rate, kRate);
    ASSERT_EQ(continuouse_params.pitch, kPitch);
    ASSERT_EQ(continuouse_params.volume, kVolume);
    ASSERT_TRUE(out_utterance->GetShouldClearQueue());

    ASSERT_TRUE(EventTypesMatches(in_utterance->GetRequiredEventTypes(),
                                  out_utterance->GetRequiredEventTypes()));
    ASSERT_TRUE(EventTypesMatches(in_utterance->GetDesiredEventTypes(),
                                  out_utterance->GetDesiredEventTypes()));

    ASSERT_TRUE(out_utterance->ShouldAlwaysBeSpoken());
  }

  {
    // Create TtsUtterance for an Ash Utterance.
    std::unique_ptr<content::TtsUtterance> out_utterance =
        tts_crosapi_util::CreateUtteranceFromMojo(
            mojo_utterance, /*should_always_be_spoken=*/false);

    ASSERT_EQ(out_utterance->GetText(), kText);
    ASSERT_EQ(out_utterance->GetVoiceName(), kVoiceName);
    ASSERT_EQ(out_utterance->GetLang(), kLang);
    ASSERT_EQ(out_utterance->GetSrcId(), kSrcId);
    ASSERT_EQ(out_utterance->GetSrcUrl(), GURL(kSrcUrl));
    ASSERT_EQ(out_utterance->GetEngineId(), kEngineId);
    auto continuouse_params = out_utterance->GetContinuousParameters();
    ASSERT_EQ(continuouse_params.rate, kRate);
    ASSERT_EQ(continuouse_params.pitch, kPitch);
    ASSERT_EQ(continuouse_params.volume, kVolume);
    ASSERT_TRUE(out_utterance->GetShouldClearQueue());

    ASSERT_TRUE(EventTypesMatches(in_utterance->GetRequiredEventTypes(),
                                  out_utterance->GetRequiredEventTypes()));
    ASSERT_TRUE(EventTypesMatches(in_utterance->GetDesiredEventTypes(),
                                  out_utterance->GetDesiredEventTypes()));

    ASSERT_FALSE(out_utterance->ShouldAlwaysBeSpoken());
  }
}

TEST_F(TtsUtteranceMojomTest, RoundTripWithOptions) {
  std::unique_ptr<content::TtsUtterance> in_utterance =
      content::TtsUtterance::Create();
  base::Value::Dict in_options;
  in_options.Set(tts_extension_api_constants::kVoiceNameKey, kVoiceName);
  in_options.Set(tts_extension_api_constants::kRateKey, kRate);
  in_options.Set(tts_extension_api_constants::kEnqueueKey, true);
  in_utterance->SetOptions(std::move(in_options));

  auto mojo_utterance = tts_crosapi_util::ToMojo(in_utterance.get());
  std::unique_ptr<content::TtsUtterance> out_utterance =
      tts_crosapi_util::CreateUtteranceFromMojo(
          mojo_utterance, /*should_always_be_spoken=*/true);

  auto* out_options = out_utterance->GetOptions();

  const base::Value* voice_value =
      out_options->Find(tts_extension_api_constants::kVoiceNameKey);
  ASSERT_TRUE(voice_value);
  ASSERT_TRUE(voice_value->is_string());
  ASSERT_EQ(voice_value->GetString(), kVoiceName);

  const base::Value* rate_value =
      out_options->Find(tts_extension_api_constants::kRateKey);
  ASSERT_TRUE(rate_value);
  ASSERT_TRUE(rate_value->is_double());
  ASSERT_EQ(rate_value->GetDouble(), kRate);

  const base::Value* enqueue_value =
      out_options->Find(tts_extension_api_constants::kEnqueueKey);
  ASSERT_TRUE(enqueue_value);
  ASSERT_TRUE(enqueue_value->is_bool());
  ASSERT_TRUE(enqueue_value->GetBool());
}

TEST_F(TtsUtteranceMojomTest, WasCreatedWithNoWebContents) {
  std::unique_ptr<content::TtsUtterance> in_utterance =
      content::TtsUtterance::Create();
  ASSERT_FALSE(in_utterance->GetWebContents());
  auto mojo_utterance = tts_crosapi_util::ToMojo(in_utterance.get());
  ASSERT_FALSE(mojo_utterance->was_created_with_web_contents);
}

TEST_F(TtsUtteranceMojomTest, WasCreatedWithWebContents) {
  content::WebContents* web_contents = GetWebContents();
  std::unique_ptr<content::TtsUtterance> in_utterance =
      content::TtsUtterance::Create(web_contents);
  auto mojo_utterance = tts_crosapi_util::ToMojo(in_utterance.get());
  ASSERT_TRUE(mojo_utterance->was_created_with_web_contents);

  // Finish |in_utterance| so that it can be destructed without DCHECK
  // failure.
  in_utterance->Finish();
}
