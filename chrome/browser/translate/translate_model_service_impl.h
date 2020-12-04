// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MODEL_SERVICE_IMPL_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MODEL_SERVICE_IMPL_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/translate/core/browser/translate_model_service.h"

// Service that manages models required to support translation in the browser.
class TranslateModelServiceImpl : public KeyedService,
                                  public translate::TranslateModelService {
 public:
  TranslateModelServiceImpl();
  ~TranslateModelServiceImpl() override;

  // translate::TranslateModelService:
  base::Optional<base::File> GetLanguageDetectionModelFile() override;
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_MODEL_SERVICE_IMPL_H_
