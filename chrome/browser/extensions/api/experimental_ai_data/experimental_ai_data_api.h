// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_AI_DATA_EXPERIMENTAL_AI_DATA_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_AI_DATA_EXPERIMENTAL_AI_DATA_API_H_

#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/common/extensions/api/experimental_ai_data.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Base class for all experimental AI data functions.
class ExperimentalAiDataApiFunction : public ExtensionFunction {
 public:
  ExperimentalAiDataApiFunction();

  ExperimentalAiDataApiFunction(const ExperimentalAiDataApiFunction&) = delete;
  ExperimentalAiDataApiFunction& operator=(
      const ExperimentalAiDataApiFunction&) = delete;

 protected:
  ~ExperimentalAiDataApiFunction() override;

  // Called when data collection is complete to return a result to the
  // extension.
  void OnDataCollected(AiDataKeyedService::AiData browser_collected_data);
  bool PreRunValidation(std::string* error) override;
};

// Collects data from the user for a private AI extension.
class ExperimentalAiDataGetAiDataFunction
    : public ExperimentalAiDataApiFunction {
 public:
  ExperimentalAiDataGetAiDataFunction();

  ExperimentalAiDataGetAiDataFunction(
      const ExperimentalAiDataGetAiDataFunction&) = delete;
  ExperimentalAiDataGetAiDataFunction& operator=(
      const ExperimentalAiDataGetAiDataFunction&) = delete;

 protected:
  ~ExperimentalAiDataGetAiDataFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("experimentalAiData.getAiData",
                             EXPERIMENTALAIDATA_PRIVATE_GETAIDATA)
};

// Collects data from the user for a private AI extension. This flavor allows
// specifying the data to collect.
class ExperimentalAiDataGetAiDataWithSpecifierFunction
    : public ExperimentalAiDataApiFunction {
 public:
  ExperimentalAiDataGetAiDataWithSpecifierFunction();

  ExperimentalAiDataGetAiDataWithSpecifierFunction(
      const ExperimentalAiDataGetAiDataFunction&) = delete;
  ExperimentalAiDataGetAiDataWithSpecifierFunction& operator=(
      const ExperimentalAiDataGetAiDataFunction&) = delete;

 protected:
  ~ExperimentalAiDataGetAiDataWithSpecifierFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("experimentalAiData.getAiDataWithSpecifier",
                             EXPERIMENTALAIDATA_PRIVATE_GETAIDATAWITHSPECIFIER)
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_AI_DATA_EXPERIMENTAL_AI_DATA_API_H_
