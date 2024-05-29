// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_MANAGER_IMPL_H_
#define CHROME_BROWSER_AI_AI_MANAGER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"

// The browser-side implementation of `blink::mojom::AIManager`, it should be
// destroyed together with the associated RFH or when the RFH is used for a
// cross-document navigation.
class AIManagerImpl : public content::DocumentUserData<AIManagerImpl>,
                      public blink::mojom::AIManager {
 public:
  AIManagerImpl(const AIManagerImpl&) = delete;
  AIManagerImpl& operator=(const AIManagerImpl&) = delete;

  ~AIManagerImpl() override;

  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<blink::mojom::AIManager> receiver);

 private:
  friend class DocumentUserData<AIManagerImpl>;
  FRIEND_TEST_ALL_PREFIXES(AIManagerImplTest,
                           NoUAFWithInvalidOnDeviceModelPath);
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit AIManagerImpl(content::RenderFrameHost* rfh);

  // `blink::mojom::AIManager` implementation.
  void CanCreateTextSession(CanCreateTextSessionCallback callback) override;
  void CreateTextSession(
      mojo::PendingReceiver<::blink::mojom::AITextSession> receiver,
      blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
      CreateTextSessionCallback callback) override;
  void GetDefaultTextSessionSamplingParams(
      GetDefaultTextSessionSamplingParamsCallback callback) override;

  void OnModelPathValidationComplete(const std::string& model_path,
                                     bool is_valid_path);

  void CanOptimizationGuideKeyedServiceCreateGenericSession(
      CanCreateTextSessionCallback callback);

  base::WeakPtr<content::BrowserContext> browser_context_;
  mojo::Receiver<blink::mojom::AIManager> receiver_{this};

  base::WeakPtrFactory<AIManagerImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_MANAGER_IMPL_H_
