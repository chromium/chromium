// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace speech {

namespace {

base::flat_map<std::string, std::string> GetRoutingParamsForUrl(
    const GURL& url) {
  ChromeSpeechRecognitionManagerDelegate delegate;
  // `GetRoutingParams()` is protected in
  // `ChromeSpeechRecognitionManagerDelegate` and public in the content
  // interface.
  content::SpeechRecognitionManagerDelegate& content_delegate = delegate;
  return content_delegate.GetRoutingParams(url::Origin::Create(url));
}

}  // namespace

TEST(ChromeSpeechRecognitionManagerDelegateTest, NtpRoutingParams) {
  base::test::ScopedFeatureList feature_list(
      omnibox::kVoiceSearchCoherenceSearchbox);

  base::flat_map<std::string, std::string> expected = {
      {"client", "chrome-ntp"},
      {"condition", "search"},
  };
  EXPECT_EQ(expected,
            GetRoutingParamsForUrl(GURL(chrome::kChromeUINewTabPageURL)));
}

TEST(ChromeSpeechRecognitionManagerDelegateTest,
     NoRoutingParamsWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kVoiceSearchCoherenceSearchbox);

  EXPECT_TRUE(
      GetRoutingParamsForUrl(GURL(chrome::kChromeUINewTabPageURL)).empty());
}

TEST(ChromeSpeechRecognitionManagerDelegateTest,
     NoRoutingParamsForOtherOrigins) {
  base::test::ScopedFeatureList feature_list(
      omnibox::kVoiceSearchCoherenceSearchbox);

  EXPECT_TRUE(GetRoutingParamsForUrl(GURL("https://www.example.com/")).empty());
}

}  // namespace speech
