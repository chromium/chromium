// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_manager.h"

#include <utility>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_view_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/common/chrome_features.h"
#include "components/gcm_driver/crypto/rfc8188_util.h"
#include "components/tabs/public/tab_interface.h"
#include "crypto/keypair.h"
#include "crypto/secure_util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace {

constexpr size_t kMaxFileTokenBytes = 4096;

std::optional<std::vector<uint8_t>> EncryptScreenshotPayloadOnBackgroundThread(
    std::vector<uint8_t> jpeg_data,
    std::vector<uint8_t> public_key,
    std::vector<uint8_t> auth_secret) {
  crypto::keypair::PrivateKey temp_key =
      crypto::keypair::PrivateKey::GenerateEcP256();

  // Encrypts according to RFC 8291 (Web Push Message Encryption), generating
  // an ephemeral ECDH P-256 header combined with RFC 8188 record encryption.
  auto encrypted_payload = gcm::EncryptPayloadWithRfc8188(
      base::as_string_view(jpeg_data), base::as_string_view(public_key),
      base::as_string_view(auth_secret), temp_key);

  // Use SecureZeroBuffer to ensure sensitive authentication material is wiped
  // without being optimized away by compiler dead-store elimination.
  crypto::SecureZeroBuffer(auth_secret);

  if (!encrypted_payload.has_value()) {
    return std::nullopt;
  }

  return base::ToVector(base::as_byte_span(*encrypted_payload));
}

}  // namespace

namespace glic {

GlicExperimentalTriggeringManager::GlicExperimentalTriggeringManager(
    GlicInstance* instance,
    GlicSharingManagerInternal* sharing_manager)
    : instance_(instance), sharing_manager_(sharing_manager) {}

GlicExperimentalTriggeringManager::~GlicExperimentalTriggeringManager() =
    default;

void GlicExperimentalTriggeringManager::Bind(
    mojo::PendingRemote<glic::mojom::ExperimentalTriggeringClient> client) {
  client_.reset();
  if (client.is_valid()) {
    client_.Bind(std::move(client));
  }
}

void GlicExperimentalTriggeringManager::GetExperimentalTriggeringUpdates(
    mojo::PendingRemote<glic::mojom::ExperimentalTriggeringUpdatesHandler>
        handler,
    base::OnceCallback<void(bool)> success_status_callback) {
  if (!client_.is_bound()) {
    std::move(success_status_callback).Run(false);
    return;
  }
  client_->GetExperimentalTriggeringUpdates(std::move(handler),
                                            std::move(success_status_callback));
}

void GlicExperimentalTriggeringManager::CaptureAndUploadEncryptedScreenshot(
    const std::vector<uint8_t>& public_key,
    const std::vector<uint8_t>& auth_secret,
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  if (!base::FeatureList::IsEnabled(
          features::kGlicExperimentalTriggeringScreenshot)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (!client_.is_bound()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  tabs::TabInterface* tab = nullptr;
  if (glic::GlicActorTaskManager* task_manager =
          instance_->GetActorTaskManager()) {
    std::vector<tabs::TabInterface*> target_tabs =
        task_manager->GetLastActedTabs();
    if (!target_tabs.empty()) {
      tab = glic::GetMostRecentlyActiveTab(target_tabs);
    }
  }

  if (!tab) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  glic::mojom::TabContextOptions options;
  options.viewport_screenshot = true;

  sharing_manager_->GetContextForActorFromTab(
      tab->GetHandle(), options,
      base::BindOnce(
          &GlicExperimentalTriggeringManager::OnPageContextFetchedForEncryption,
          weak_ptr_factory_.GetWeakPtr(), public_key, auth_secret,
          std::move(callback)));
}

void GlicExperimentalTriggeringManager::OnPageContextFetchedForEncryption(
    std::vector<uint8_t> public_key,
    std::vector<uint8_t> auth_secret,
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    glic::GlicGetContextResult result) {
  if (!result.has_value() || !result.value() ||
      !result.value()->is_tab_context()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  const auto& tab_context = result.value()->get_tab_context();
  if (!tab_context || !tab_context->viewport_screenshot) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  uint32_t width_pixels = tab_context->viewport_screenshot->width_pixels;
  uint32_t height_pixels = tab_context->viewport_screenshot->height_pixels;
  std::string mime_type = tab_context->viewport_screenshot->mime_type;
  auto origin_annotations =
      tab_context->viewport_screenshot->origin_annotations.Clone();
  std::vector<uint8_t> jpeg_data =
      std::move(tab_context->viewport_screenshot->data);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&EncryptScreenshotPayloadOnBackgroundThread,
                     std::move(jpeg_data), std::move(public_key),
                     std::move(auth_secret)),
      base::BindOnce(&GlicExperimentalTriggeringManager::OnScreenshotEncrypted,
                     weak_ptr_factory_.GetWeakPtr(), width_pixels,
                     height_pixels, std::move(mime_type),
                     std::move(origin_annotations), std::move(callback)));
}

void GlicExperimentalTriggeringManager::OnScreenshotEncrypted(
    uint32_t width_pixels,
    uint32_t height_pixels,
    std::string mime_type,
    glic::mojom::ImageOriginAnnotationsPtr origin_annotations,
    base::OnceCallback<void(const std::optional<std::string>&)> callback,
    std::optional<std::vector<uint8_t>> encrypted_payload_bytes) {
  if (!encrypted_payload_bytes.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto screenshot_ptr = glic::mojom::Screenshot::New();
  screenshot_ptr->width_pixels = width_pixels;
  screenshot_ptr->height_pixels = height_pixels;
  screenshot_ptr->data = std::move(*encrypted_payload_bytes);
  screenshot_ptr->mime_type = std::move(mime_type);
  screenshot_ptr->origin_annotations = std::move(origin_annotations);
  screenshot_ptr->encryption_scheme =
      glic::mojom::ScreenshotEncryptionScheme::kRfc8291;

  if (!client_.is_bound()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  client_->UploadEncryptedScreenshot(
      std::move(screenshot_ptr),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              [](base::OnceCallback<void(const std::optional<std::string>&)>
                     callback,
                 const std::optional<std::string>& file_token) {
                bool success =
                    file_token && file_token->size() <= kMaxFileTokenBytes;
                base::UmaHistogramBoolean(
                    "Glic.Actor.EncryptedScreenshotUploadSuccess", success);
                if (!success) {
                  if (!file_token) {
                    DLOG(WARNING)
                        << "Glic Web Client failed to upload encrypted "
                           "screenshot (null token returned).";
                  } else if (file_token->size() > kMaxFileTokenBytes) {
                    DLOG(ERROR)
                        << "Excessively long file token received from Glic Web "
                           "Client: "
                        << file_token->size() << " bytes (max "
                        << kMaxFileTokenBytes << ").";
                  }
                  std::move(callback).Run(std::nullopt);
                  return;
                }
                std::move(callback).Run(file_token);
              },
              std::move(callback)),
          std::nullopt));
}

}  // namespace glic
