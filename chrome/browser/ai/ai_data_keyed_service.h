// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_H_
#define CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

// Browser service to collect AI data.
class AiDataKeyedService : public KeyedService {
 public:
  // Data related to AiData collection.
  using BrowserData = optimization_guide::proto::BrowserCollectedInformation;
  using AiData = std::optional<BrowserData>;
  using AiDataCallback = base::OnceCallback<void(AiData)>;

  explicit AiDataKeyedService(content::BrowserContext* browser_context);
  AiDataKeyedService(const AiDataKeyedService&) = delete;
  AiDataKeyedService& operator=(const AiDataKeyedService&) = delete;
  ~AiDataKeyedService() override;

  static std::vector<std::string> GetAllowlistedExtensions();

  // Fills an AiData and returns the result via the passed in callback. If the
  // AiData is empty, data collection failed. |callback| is guaranteed to be
  // called, and guaranteed to be called asynchronously. This method uses a set
  // of default specifiers. See |GetAiDataWithSpecifiers| for more details.
  void GetAiData(int dom_node_id,
                 content::WebContents* web_contents,
                 std::string user_input,
                 AiDataCallback callback);

  // Fills an AiData and returns the result via the passed in callback. If the
  // AiData is empty, data collection failed. |callback| is guaranteed to be
  // called, and guaranteed to be called asynchronously.
  // |tabs_for_inner_text|: The number of tabs to collect inner text for.
  void GetAiDataWithSpecifiers(int tabs_for_inner_text,
                               int dom_node_id,
                               content::WebContents* web_contents,
                               std::string user_input,
                               AiDataCallback callback);

  static const base::Feature& GetAllowlistedAiDataExtensionsFeatureForTesting();

 private:
  // A `KeyedService` should never outlive the `BrowserContext`.
  raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtrFactory<AiDataKeyedService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_H_
