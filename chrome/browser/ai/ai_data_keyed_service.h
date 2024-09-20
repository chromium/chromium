// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_H_
#define CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_H_

#include <optional>

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
  using AiData =
      std::optional<optimization_guide::proto::
                        ModelPrototypingRequest_BrowserCollectedInformation>;
  using AiDataCallback = base::OnceCallback<void(AiData)>;

  explicit AiDataKeyedService(content::BrowserContext* browser_context);
  AiDataKeyedService(const AiDataKeyedService&) = delete;
  AiDataKeyedService& operator=(const AiDataKeyedService&) = delete;
  ~AiDataKeyedService() override;

  // Fills an AiData and returns the result via the passed in callback. If the
  // AiData is empty, data collection failed. |callback| is guaranteed to be
  // called, and guaranteed to be called asynchronously.
  void GetAiData(int dom_node_id,
                 content::WebContents* web_contents,
                 std::string user_input,
                 AiDataCallback callback);

  // A `KeyedService` should never outlive the `BrowserContext`.
  raw_ptr<content::BrowserContext> browser_context_;

  base::WeakPtrFactory<AiDataKeyedService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_H_
