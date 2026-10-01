// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_AI_DATA_EXPERIMENTAL_AI_DATA_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_AI_DATA_EXPERIMENTAL_AI_DATA_API_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/common/extensions/api/experimental_ai_data.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/screenshot_access.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

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

  // Observe the target page until the response, plus the first
  // `max_tabs_for_text_collection` tabs in tab order whose text the service
  // will collect.
  // Other tabs contribute only metadata and do not need page guards.
  std::optional<std::string> StartDataCollection(
      content::WebContents* web_contents,
      int max_tabs_for_text_collection = 0);
  std::optional<std::string> GetDataCollectionError() const;
  content::WebContents* GetTargetWebContents() const;

  // Called when data collection is complete to return a result to the
  // extension.
  void OnDataCollected(AiDataKeyedService::AiData browser_collected_data);
  bool PreRunValidation(std::string* error) override;

 private:
  class CollectionPageGuard;
  std::vector<std::unique_ptr<CollectionPageGuard>> collection_page_guards_;
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

// Returns a coordinated Annotated Page Content (APC) extraction, viewport
// screenshot, or both to the allowlisted extension.
class ExperimentalAiDataGetApcSnapshotFunction
    : public ExperimentalAiDataApiFunction {
 public:
  ExperimentalAiDataGetApcSnapshotFunction();

  ExperimentalAiDataGetApcSnapshotFunction(
      const ExperimentalAiDataGetApcSnapshotFunction&) = delete;
  ExperimentalAiDataGetApcSnapshotFunction& operator=(
      const ExperimentalAiDataGetApcSnapshotFunction&) = delete;

 protected:
  ~ExperimentalAiDataGetApcSnapshotFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Resolves the target tab, excluding incognito tabs. Virtual so
  // unit tests can supply a WebContents without a browser window.
  virtual content::WebContents* GetTabById(int tab_id, bool include_incognito);

  // Checks screenshot preferences and DLP restrictions. Virtual so unit tests
  // can simulate restrictions changing during capture.
  virtual base::expected<void, ScreenshotAccessError> CheckScreenshotAccess(
      content::WebContents* web_contents) const;

  // Fetches page content and any requested screenshot. Virtual so unit tests
  // can control completion, failures, and changes to the page during capture.
  virtual void FetchSnapshot(
      content::WebContents* web_contents,
      const page_content_annotations::FetchPageContextOptions& options,
      page_content_annotations::FetchPageContextResultCallback callback);

 private:
  // Configures APC extraction and optional lossless viewport capture, then
  // starts the fetch with asynchronous completion. Screenshot-only requests
  // also extract APC for redaction.
  void StartSnapshot(content::WebContents* web_contents,
                     bool capture_apc,
                     bool capture_screenshot,
                     blink::mojom::AIPageContentOptionsPtr apc_options);

  // Handles fetch errors or starts serialization on a worker thread.
  void OnSnapshotFetched(
      bool capture_apc,
      bool capture_screenshot,
      page_content_annotations::FetchPageContextResultCallbackArg result);

  // Rechecks page validity and permissions before returning serialized data.
  void OnSnapshotSerialized(
      bool capture_screenshot,
      base::expected<base::ListValue, std::string> result);

  DECLARE_EXTENSION_FUNCTION("experimentalAiData.getApcSnapshot",
                             EXPERIMENTALAIDATA_GETAPCSNAPSHOT)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXPERIMENTAL_AI_DATA_EXPERIMENTAL_AI_DATA_API_H_
