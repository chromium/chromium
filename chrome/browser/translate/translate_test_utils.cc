// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_test_utils.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "content/public/browser/web_contents.h"

namespace translate {

std::unique_ptr<TranslateWaiter> CreateTranslateWaiter(
    content::WebContents* web_contents,
    TranslateWaiter::WaitEvent wait_event) {
  return std::make_unique<TranslateWaiter>(
      ChromeTranslateClient::FromWebContents(web_contents)->translate_driver(),
      wait_event);
}

}  // namespace translate
