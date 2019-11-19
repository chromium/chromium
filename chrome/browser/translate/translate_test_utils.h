// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_TEST_UTILS_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_TEST_UTILS_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/core/common/translate_errors.h"

namespace content {
class WebContents;
}

namespace translate {

// A helper class that allows test to block until certain translate events have
// been received from a tab's ContentTranslateDriver.
class TranslateWaiter : ContentTranslateDriver::Observer {
 public:
  enum class WaitEvent {
    kLanguageDetermined,
    kPageTranslated,
  };

  TranslateWaiter(content::WebContents* web_contents, WaitEvent wait_event);
  ~TranslateWaiter() override;

  // Blocks until an observer function matching |wait_event_| is invoked, or
  // returns immediately if one has already been observed.
  void Wait();

  // ContentTranslateDriver::Observer:
  void OnLanguageDetermined(const LanguageDetectionDetails& details) override;
  void OnPageTranslated(const std::string& original_lang,
                        const std::string& translated_lang,
                        TranslateErrors::Type error_type) override;

 private:
  WaitEvent wait_event_;
  ScopedObserver<ContentTranslateDriver, ContentTranslateDriver::Observer>
      scoped_observer_{this};
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TranslateWaiter);
};

}  // namespace translate

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_TEST_UTILS_H_
