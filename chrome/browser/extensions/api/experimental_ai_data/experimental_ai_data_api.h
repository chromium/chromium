// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_AI_DATA_EXPERIMENTAL_AI_DATA_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_AI_DATA_EXPERIMENTAL_AI_DATA_API_H_

#include "base/feature_list.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

BASE_DECLARE_FEATURE(kAllowlistedAiDataExtensions);

// Collects data from the user for a private AI extension.
class ExperimentalAiDataGetAiDataFunction : public ExtensionFunction {
 public:
  ExperimentalAiDataGetAiDataFunction();

  ExperimentalAiDataGetAiDataFunction(
      const ExperimentalAiDataGetAiDataFunction&) = delete;
  ExperimentalAiDataGetAiDataFunction& operator=(
      const ExperimentalAiDataGetAiDataFunction&) = delete;

 protected:
  ~ExperimentalAiDataGetAiDataFunction() override;

  ResponseAction Run() override;
  void OnDataCollected(AiDataKeyedService::AiData browser_collected_data);

  DECLARE_EXTENSION_FUNCTION("experimentalAiData.getAiData",
                             EXPERIMENTALAIDATA_PRIVATE_GETAIDATA)
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_AI_DATA_EXPERIMENTAL_AI_DATA_API_H_
