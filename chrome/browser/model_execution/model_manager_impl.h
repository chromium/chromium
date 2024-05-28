// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MODEL_EXECUTION_MODEL_MANAGER_IMPL_H_
#define CHROME_BROWSER_MODEL_EXECUTION_MODEL_MANAGER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/model_execution/model_manager.mojom.h"

// The browser-side implementation of `blink::mojom::ModelManager`, it should be
// destroyed together with the associated RFH or when the RFH is used for a
// cross-document navigation.
class ModelManagerImpl : public content::DocumentUserData<ModelManagerImpl>,
                         public blink::mojom::ModelManager {
 public:
  ModelManagerImpl(const ModelManagerImpl&) = delete;
  ModelManagerImpl& operator=(const ModelManagerImpl&) = delete;

  ~ModelManagerImpl() override;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ModelManager> receiver);

 private:
  friend class DocumentUserData<ModelManagerImpl>;
  FRIEND_TEST_ALL_PREFIXES(ModelManagerImplTest,
                           NoUAFWithInvalidOnDeviceModelPath);
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit ModelManagerImpl(content::RenderFrameHost* rfh);

  // `blink::mojom::ModelManager` implementation.
  void CanCreateGenericSession(
      CanCreateGenericSessionCallback callback) override;
  void CreateGenericSession(
      mojo::PendingReceiver<::blink::mojom::ModelGenericSession> receiver,
      blink::mojom::ModelGenericSessionSamplingParamsPtr sampling_params,
      CreateGenericSessionCallback callback) override;
  void GetDefaultGenericSessionSamplingParams(
      GetDefaultGenericSessionSamplingParamsCallback callback) override;

  void OnModelPathValidationComplete(CanCreateGenericSessionCallback callback,
                                     const std::string& model_path,
                                     bool is_valid_path);

  void CanOptimizationGuideKeyedServiceCreateGenericSession(
      CanCreateGenericSessionCallback callback);

  base::WeakPtr<content::BrowserContext> browser_context_;
  mojo::Receiver<blink::mojom::ModelManager> receiver_{this};

  base::WeakPtrFactory<ModelManagerImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MODEL_EXECUTION_MODEL_MANAGER_IMPL_H_
