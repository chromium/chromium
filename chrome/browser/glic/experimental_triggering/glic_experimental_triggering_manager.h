// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MANAGER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace glic {
class GlicInstance;
class GlicSharingManagerInternal;

class GlicExperimentalTriggeringManager {
 public:
  GlicExperimentalTriggeringManager(
      GlicInstance* instance,
      GlicSharingManagerInternal* sharing_manager);
  GlicExperimentalTriggeringManager(const GlicExperimentalTriggeringManager&) =
      delete;
  GlicExperimentalTriggeringManager& operator=(
      const GlicExperimentalTriggeringManager&) = delete;
  ~GlicExperimentalTriggeringManager();

  void Bind(
      mojo::PendingRemote<glic::mojom::ExperimentalTriggeringClient> client);

  void CaptureAndUploadEncryptedScreenshot(
      const std::vector<uint8_t>& public_key,
      const std::vector<uint8_t>& auth_secret,
      base::OnceCallback<void(const std::optional<std::string>&)> callback);

  void GetExperimentalTriggeringUpdates(
      mojo::PendingRemote<glic::mojom::ExperimentalTriggeringUpdatesHandler>
          handler,
      base::OnceCallback<void(bool)> success_status_callback);

 private:
  void OnPageContextFetchedForEncryption(
      std::vector<uint8_t> public_key,
      std::vector<uint8_t> auth_secret,
      base::OnceCallback<void(const std::optional<std::string>&)> callback,
      glic::GlicGetContextResult result);
  void OnScreenshotEncrypted(
      uint32_t width_pixels,
      uint32_t height_pixels,
      std::string mime_type,
      glic::mojom::ImageOriginAnnotationsPtr origin_annotations,
      base::OnceCallback<void(const std::optional<std::string>&)> callback,
      std::optional<std::vector<uint8_t>> encrypted_payload_bytes);

  raw_ptr<glic::GlicInstance> instance_;
  raw_ptr<glic::GlicSharingManagerInternal> sharing_manager_;
  mojo::Remote<glic::mojom::ExperimentalTriggeringClient> client_;
  base::WeakPtrFactory<GlicExperimentalTriggeringManager> weak_ptr_factory_{
      this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_TRIGGERING_GLIC_EXPERIMENTAL_TRIGGERING_MANAGER_H_
