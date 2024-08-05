// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_H_
#define CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_text_session.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom-forward.h"

// The browser-side implementation of `blink::mojom::AIManager`. There should be
// one shared AIManagerKeyedService per BrowserContext.
class AIManagerKeyedService : public KeyedService,
                              public blink::mojom::AIManager {
 public:
  explicit AIManagerKeyedService(content::BrowserContext* browser_context);
  AIManagerKeyedService(const AIManagerKeyedService&) = delete;
  AIManagerKeyedService& operator=(const AIManagerKeyedService&) = delete;

  ~AIManagerKeyedService() override;

  void AddReceiver(mojo::PendingReceiver<blink::mojom::AIManager> receiver);
  void CreateTextSessionForCloning(
      base::PassKey<AITextSession> pass_key,
      mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
      blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
      const AITextSession::Context& context,
      base::OnceCallback<void(bool)> callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(AIManagerKeyedServiceTest,
                           NoUAFWithInvalidOnDeviceModelPath);

  // `blink::mojom::AIManager` implementation.
  void CanCreateTextSession(CanCreateTextSessionCallback callback) override;
  void CreateTextSession(
      mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
      blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
      const std::optional<std::string>& system_prompt,
      CreateTextSessionCallback callback) override;
  void GetTextModelInfo(GetTextModelInfoCallback callback) override;

  void OnModelPathValidationComplete(const std::string& model_path,
                                     bool is_valid_path);

  void CanOptimizationGuideKeyedServiceCreateGenericSession(
      CanCreateTextSessionCallback callback);

  // Creates an `AITextSession`, either as a new session, or as a clone of an
  // existing session with its context copied.
  std::unique_ptr<AITextSession> CreateTextSessionInternal(
      const blink::mojom::AITextSessionSamplingParamsPtr& sampling_params,
      const std::optional<const AITextSession::Context>& context =
          std::nullopt);

  // A `KeyedService` should never outlive the `BrowserContext`.
  raw_ptr<content::BrowserContext> browser_context_;

  mojo::ReceiverSet<blink::mojom::AIManager> receivers_;

  base::WeakPtrFactory<AIManagerKeyedService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_H_
