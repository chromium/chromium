// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_TAB_HELPER_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/translate/core/browser/translate_driver.h"

class ChromeTranslateClient;

namespace accessibility_annotator {

class ContentAnnotatorService;

class ContentAnnotatorTabHelper
    : public tabs::ContentsObservingTabFeature,
      public translate::TranslateDriver::LanguageDetectionObserver {
 public:
  explicit ContentAnnotatorTabHelper(
      tabs::TabInterface& tab,
      ContentAnnotatorService& content_annotator_service,
      ChromeTranslateClient* chrome_translate_client);
  ContentAnnotatorTabHelper(const ContentAnnotatorTabHelper&) = delete;
  ContentAnnotatorTabHelper& operator=(const ContentAnnotatorTabHelper&) =
      delete;

  ~ContentAnnotatorTabHelper() override;

  // TranslateDriver::LanguageDetectionObserver implementation.
  void OnTranslateDriverDestroyed(translate::TranslateDriver* driver) override;
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

 private:
  // Observes LanguageDetectionObserver, which notifies us when the language of
  // the contents of the current page has been determined.
  base::ScopedObservation<translate::TranslateDriver,
                          translate::TranslateDriver::LanguageDetectionObserver>
      translate_observation_{this};

  // ContentAnnotatorService to forward language detection events to.
  const raw_ref<ContentAnnotatorService> content_annotator_service_;

  // ChromeTranslateClient to observe for language detection events.
  const raw_ptr<ChromeTranslateClient> chrome_translate_client_;
};
}  // namespace accessibility_annotator

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANNOTATOR_CONTENT_ANNOTATOR_CONTENT_ANNOTATOR_TAB_HELPER_H_
