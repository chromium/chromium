// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_test_utils.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "content/public/browser/web_contents.h"

namespace translate {

TranslateWaiter::TranslateWaiter(content::WebContents* web_contents,
                                 WaitEvent wait_event)
    : wait_event_(wait_event) {
  scoped_observer_.Add(
      ChromeTranslateClient::FromWebContents(web_contents)->translate_driver());
}

TranslateWaiter::~TranslateWaiter() = default;

void TranslateWaiter::Wait() {
  run_loop_.Run();
}

// ContentTranslateDriver::Observer:
void TranslateWaiter::OnLanguageDetermined(
    const LanguageDetectionDetails& details) {
  if (wait_event_ == WaitEvent::kLanguageDetermined)
    run_loop_.Quit();
}

void TranslateWaiter::OnPageTranslated(const std::string& original_lang,
                                       const std::string& translated_lang,
                                       TranslateErrors::Type error_type) {
  if (wait_event_ == WaitEvent::kPageTranslated)
    run_loop_.Quit();
}

}  // namespace translate
