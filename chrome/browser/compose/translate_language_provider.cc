// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/translate_language_provider.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "translate_language_provider.h"

TranslateLanguageProvider::~TranslateLanguageProvider() = default;

bool TranslateLanguageProvider::IsLanguageSupported(
    translate::TranslateManager* translate_manager) {
  std::string source_language = GetSourceLanguage(translate_manager);
  // TODO(b/307814938): Make this finch configurable.
  // Only English is supported for MVP, we will add more languages over time.
  return (source_language == std::string("en"));
}

std::string TranslateLanguageProvider::GetSourceLanguage(
    translate::TranslateManager* translate_manager) {
  if (translate_manager == nullptr) {
    return std::string();
  }

  return translate_manager->GetLanguageState()->source_language();
}
