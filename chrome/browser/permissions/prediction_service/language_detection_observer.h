// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_LANGUAGE_DETECTION_OBSERVER_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_LANGUAGE_DETECTION_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/translate/core/browser/translate_driver.h"
#include "content/public/browser/web_contents.h"

class ChromeTranslateClient;

namespace permissions {

// An observer with timeout that waits for the page's language to be determined.
//
// This class checks if the language of the provided webcontents page is
// already known to be English. If so, it synchronously invokes the success
// callback. If the language is known and is not English, it invokes the
// fallback callback.
//
// If the language is not yet determined, it registers itself as a
// LanguageDetectionObserver using ChromeTranslateDriver class and will
// unregister itself as an observer after either a successful detection, a
// non-English detection, or a timeout. It then waits for language
// determination, invoking the appropriate callback based on whether the
// detected language is English.
//
// A timeout is used to limit the wait time for language detection. If
// the language is not determined within the timeout period, the fallback
// callback is invoked and the observer is deregistered.
class LanguageDetectionObserver
    : public translate::TranslateDriver::LanguageDetectionObserver {
 public:
  LanguageDetectionObserver();
  ~LanguageDetectionObserver() override;

  // The timeout for the language detection in seconds. If the language
  // detection takes longer than this, the fallback callback will be invoked.
  static const int kLanguageDetectionTimeout = 1;

  // Virtual for testing.
  virtual void Init(content::WebContents* web_contents,
                    base::OnceCallback<void()> on_english_detected,
                    base::OnceCallback<void()> on_fallback);

  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

  void Reset();

  bool WaitingForLanguageDetection();

 protected:
  void OnTimeout();

  // Virtual for testing.
  virtual void RemoveAsObserver();

  ChromeTranslateClient* chrome_translate_client();

  raw_ptr<content::WebContents> web_contents_;

  // Called when English is determined to be the language of the current
  // webcontents.
  base::OnceCallback<void()> on_english_detected_callback_;

  // Called when language detection takes longer than the timeout or when the
  // detected language is not English.
  base::OnceCallback<void()> fallback_callback_;

  base::OneShotTimer timeout_timer_;

  // Used for the timer to bind OnTimeout as a callback.
  base::WeakPtrFactory<LanguageDetectionObserver> weak_ptr_factory_{this};
};
}  // namespace permissions

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_LANGUAGE_DETECTION_OBSERVER_H_
