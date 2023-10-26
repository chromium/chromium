// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_TRANSLATE_LANGUAGE_PROVIDER_H_
#define CHROME_BROWSER_COMPOSE_TRANSLATE_LANGUAGE_PROVIDER_H_

#include <string>

#include "components/translate/core/browser/translate_manager.h"

class TranslateLanguageProvider {
 public:
  virtual std::string GetSourceLanguage(
      translate::TranslateManager* translate_manager);
  bool IsLanguageSupported(translate::TranslateManager* translate_manager);
  virtual ~TranslateLanguageProvider();
};

#endif  // CHROME_BROWSER_COMPOSE_TRANSLATE_LANGUAGE_PROVIDER_H_
