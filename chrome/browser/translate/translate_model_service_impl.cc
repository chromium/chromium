// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_model_service_impl.h"

#include "base/files/file.h"
#include "base/optional.h"

TranslateModelServiceImpl::TranslateModelServiceImpl() {
  // TODO(crbug.com/1151407): Register with the Optimiziation Guide for the
  // language detection model.
}

TranslateModelServiceImpl::~TranslateModelServiceImpl() = default;

base::Optional<base::File>
TranslateModelServiceImpl::GetLanguageDetectionModelFile() {
  // TODO(crbug.com/1151406): Implement loading the model on a background thread
  // and return it for use by translate.
  return base::nullopt;
}
