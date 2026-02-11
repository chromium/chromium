// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_tab_helper.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"

namespace accessibility_annotator {

ContentAnnotatorTabHelper::ContentAnnotatorTabHelper(
    tabs::TabInterface& tab,
    ContentAnnotatorService& content_annotator_service,
    ChromeTranslateClient* chrome_translate_client)
    : ContentsObservingTabFeature(tab),
      content_annotator_service_(content_annotator_service),
      chrome_translate_client_(chrome_translate_client) {
  // A translate client is not always attached to web contents (e.g. tests).
  if (chrome_translate_client_) {
    translate_observation_.Observe(
        chrome_translate_client_->GetTranslateDriver());
  }
}

ContentAnnotatorTabHelper::~ContentAnnotatorTabHelper() = default;

void ContentAnnotatorTabHelper::OnTranslateDriverDestroyed(
    translate::TranslateDriver* driver) {
  translate_observation_.Reset();
}

void ContentAnnotatorTabHelper::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  content_annotator_service_->OnLanguageDetermined(details);
}

}  // namespace accessibility_annotator
