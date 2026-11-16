// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/glic/experimental_triggering/glic_experimental_triggering_manager.h"

#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/gcm_driver/crypto/rfc8291_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
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
  auto encrypted_payload = gcm::EncryptPayloadWithRfc8291(
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
    : instance_(instance), sharing_manager_(sharing_manager) {
  if (instance_) {
    instance_will_be_destroyed_subscription_ =
        instance_->RegisterWillBeDestroyed(base::BindOnce(
            &GlicExperimentalTriggeringManager::OnInstanceWillBeDestroyed,
            weak_ptr_factory_.GetWeakPtr()));
  }
  if (sharing_manager_) {
    tab_pinning_status_subscription_ =
        sharing_manager_->AddTabPinningStatusEventCallback(base::BindRepeating(
            &GlicExperimentalTriggeringManager::OnTabPinningStatusEvent,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

GlicExperimentalTriggeringManager::~GlicExperimentalTriggeringManager() =
    default;

void GlicExperimentalTriggeringManager::Bind(
    mojo::PendingRemote<glic::mojom::ExperimentalTriggeringClient> client) {
  client_.reset();
  if (client.is_valid()) {
    client_.Bind(std::move(client));
    client_.reset_on_disconnect();
    NotifyClientBound();
  }
}

bool GlicExperimentalTriggeringManager::IsClientBound() const {
  return client_.is_bound() && client_.is_connected();
}

base::CallbackListSubscription
GlicExperimentalTriggeringManager::AddClientBoundCallback(
    base::RepeatingClosure callback) {
  return client_bound_callbacks_.Add(std::move(callback));
}

void GlicExperimentalTriggeringManager::NotifyClientBound() {
  client_bound_callbacks_.Notify();
}

base::ScopedClosureRunner
GlicExperimentalTriggeringManager::RegisterActiveActuationSession() {
  if (!base::FeatureList::IsEnabled(
          features::kGlicExperimentalTriggeringKeepInstanceOnUnbind)) {
    return base::ScopedClosureRunner();
  }
  const bool was_active = HasActiveActuationSession();
  ++active_actuation_sessions_;
  if (!was_active) {
    active_actuation_session_changed_callbacks_.Notify(true);
  }
  return base::ScopedClosureRunner(base::BindOnce(
      &GlicExperimentalTriggeringManager::OnActiveActuationSessionEnded,
      weak_ptr_factory_.GetWeakPtr()));
}

base::CallbackListSubscription
GlicExperimentalTriggeringManager::AddActiveActuationSessionChangedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  return active_actuation_session_changed_callbacks_.Add(std::move(callback));
}

void GlicExperimentalTriggeringManager::OnActiveActuationSessionEnded() {
  CHECK_GT(active_actuation_sessions_, 0);
  --active_actuation_sessions_;
  if (active_actuation_sessions_ != 0) {
    return;
  }
  // Notify first: a blank instance whose closed side panel is still bound
  // removes itself here (unpinning its own tab), so only tabs it leaves behind
  // need the deferred unpin below.
  active_actuation_session_changed_callbacks_.Notify(false);
  UnpinDeferredTabs();
}

bool GlicExperimentalTriggeringManager::DeferUnusedTabUnpinOnUnbind(
    tabs::TabHandle tab_handle) {
  if (!HasActiveActuationSession()) {
    return false;
  }
  std::erase(deferred_unpin_tabs_, tab_handle);
  deferred_unpin_tabs_.push_back(tab_handle);
  return true;
}

tabs::TabInterface* GlicExperimentalTriggeringManager::GetLastDeferredUnpinTab()
    const {
  if (!instance_ || !sharing_manager_) {
    return nullptr;
  }
  for (tabs::TabHandle tab_handle : base::Reversed(deferred_unpin_tabs_)) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (!tab || !sharing_manager_->IsTabPinned(tab_handle)) {
      continue;
    }
    // Another conversation adopted the tab since it was unbound from this one;
    // continuing here must not pull it back.
    auto* helper = GlicInstanceHelper::From(tab);
    if (helper && helper->GetInstanceId().has_value() &&
        helper->GetInstanceId() != instance_->id()) {
      continue;
    }
    return tab;
  }
  return nullptr;
}

void GlicExperimentalTriggeringManager::UnpinDeferredTabs() {
  std::vector<tabs::TabHandle> deferred = std::move(deferred_unpin_tabs_);
  deferred_unpin_tabs_.clear();
  // Instance teardown unpins everything itself.
  if (instance_will_be_destroyed_notified_ || !instance_ || !sharing_manager_) {
    return;
  }
  std::vector<tabs::TabHandle> to_unpin;
  for (tabs::TabHandle tab_handle : deferred) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (!tab || !sharing_manager_->IsTabPinned(tab_handle)) {
      continue;
    }
    // Bound again since the unbind that deferred it: the binding owns the pin.
    if (auto* helper = GlicInstanceHelper::From(tab);
        helper && helper->GetInstanceId() == instance_->id()) {
      continue;
    }
    std::optional<GlicPinnedTabUsage> usage =
        sharing_manager_->GetPinnedTabUsage(tab_handle);
    if (!usage || usage->pin_event.trigger == GlicPinTrigger::kRestore ||
        !usage->Unused() || usage->IsExplicitlyPinnedByUser()) {
      continue;
    }
    to_unpin.push_back(tab_handle);
  }
  if (!to_unpin.empty()) {
    sharing_manager_->UnpinTabs(to_unpin);
  }
}

void GlicExperimentalTriggeringManager::OnTabPinningStatusEvent(
    tabs::TabInterface* tab,
    GlicPinningStatusEvent event) {
  if (const auto* unpin_event = std::get_if<GlicUnpinEvent>(&event)) {
    if (tab) {
      std::erase(deferred_unpin_tabs_, tab->GetHandle());
    }
    if (unpin_event->trigger == GlicUnpinTrigger::kTabClose &&
        sharing_manager_->GetNumPinnedTabs() == 0) {
      last_pinned_tab_closed_ = true;
    }
    return;
  }
  last_pinned_tab_closed_ = false;
}

base::CallbackListSubscription
GlicExperimentalTriggeringManager::AddInstanceWillBeDestroyedCallback(
    base::RepeatingCallback<void(InstanceTeardownReason)> callback) {
  return instance_will_be_destroyed_callbacks_.Add(std::move(callback));
}

void GlicExperimentalTriggeringManager::OnInstanceWillBeDestroyed(
    GlicInstance* instance) {
  NotifyInstanceWillBeDestroyed(
      last_pinned_tab_closed_ ? InstanceTeardownReason::kTabClosed
                              : InstanceTeardownReason::kInstanceDestroyed);
}

void GlicExperimentalTriggeringManager::NotifyInstanceWillBeDestroyed(
    InstanceTeardownReason reason) {
  if (instance_will_be_destroyed_notified_) {
    return;
  }
  instance_will_be_destroyed_notified_ = true;
  instance_will_be_destroyed_callbacks_.Notify(reason);
}

void GlicExperimentalTriggeringManager::GetExperimentalTriggeringUpdates(
    mojo::PendingRemote<glic::mojom::ExperimentalTriggeringUpdatesHandler>
        handler,
    base::OnceCallback<void(bool)> success_status_callback) {
  if (!IsClientBound()) {
    std::move(success_status_callback).Run(false);
    return;
  }
  client_->GetExperimentalTriggeringUpdates(
      std::move(handler), mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                              std::move(success_status_callback), false));
}

void GlicExperimentalTriggeringManager::SubmitConfirmationResponse(
    const std::vector<uint8_t>& response,
    base::OnceCallback<void(bool)> callback) {
  if (!client_.is_bound()) {
    std::move(callback).Run(false);
    return;
  }
  client_->SubmitExperimentalTriggeringConfirmationResponse(
      response,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void GlicExperimentalTriggeringManager::CaptureAndUploadEncryptedScreenshot(
    const std::vector<uint8_t>& public_key,
    const std::vector<uint8_t>& auth_secret,
    base::OnceCallback<
        void(base::expected<std::string, ScreenshotResult::Status>)> callback) {
  if (!base::FeatureList::IsEnabled(
          features::kGlicExperimentalTriggeringScreenshot)) {
    std::move(callback).Run(
        base::unexpected(ScreenshotResult::Status::kErrorDisabled));
    return;
  }

  if (!client_.is_bound()) {
    std::move(callback).Run(
        base::unexpected(ScreenshotResult::Status::kErrorServer));
    return;
  }

  tabs::TabInterface* tab = nullptr;
  if (glic::GlicActorTaskManager* task_manager =
          instance_->GetActorTaskManager()) {
    std::vector<raw_ptr<tabs::TabInterface>> target_tabs =
        task_manager->GetLastActedTabs();
    if (!target_tabs.empty()) {
      tab = glic::GetMostRecentlyActiveTab(target_tabs);
    }
  }

  if (!tab) {
    std::move(callback).Run(
        base::unexpected(ScreenshotResult::Status::kErrorCapture));
    return;
  }

  glic::mojom::TabContextOptions options;
  options.viewport_screenshot = true;

  GlicMetricReporter reporter(
      Profile::FromBrowserContext(tab->GetContents()->GetBrowserContext()));
  sharing_manager_->GetContextForActorFromTab(
      tab->GetHandle(), options,
      base::BindOnce(
          &GlicExperimentalTriggeringManager::OnPageContextFetchedForEncryption,
          weak_ptr_factory_.GetWeakPtr(), reporter, public_key, auth_secret,
          std::move(callback)));
}

void GlicExperimentalTriggeringManager::OnPageContextFetchedForEncryption(
    GlicMetricReporter reporter,
    std::vector<uint8_t> public_key,
    std::vector<uint8_t> auth_secret,
    base::OnceCallback<
        void(base::expected<std::string, ScreenshotResult::Status>)> callback,
    glic::GlicGetContextResult result) {
  if (!result.has_value() || !result.value() ||
      !result.value()->is_tab_context()) {
    std::move(callback).Run(
        base::unexpected(ScreenshotResult::Status::kErrorCapture));
    return;
  }

  const auto& tab_context = result.value()->get_tab_context();
  if (!tab_context || !tab_context->viewport_screenshot) {
    std::move(callback).Run(
        base::unexpected(ScreenshotResult::Status::kErrorCapture));
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
                     weak_ptr_factory_.GetWeakPtr(), reporter, width_pixels,
                     height_pixels, std::move(mime_type),
                     std::move(origin_annotations), std::move(callback)));
}

void GlicExperimentalTriggeringManager::OnScreenshotEncrypted(
    GlicMetricReporter reporter,
    uint32_t width_pixels,
    uint32_t height_pixels,
    std::string mime_type,
    glic::mojom::ImageOriginAnnotationsPtr origin_annotations,
    base::OnceCallback<
        void(base::expected<std::string, ScreenshotResult::Status>)> callback,
    std::optional<std::vector<uint8_t>> encrypted_payload_bytes) {
  if (!encrypted_payload_bytes.has_value()) {
    std::move(callback).Run(
        base::unexpected(ScreenshotResult::Status::kErrorCapture));
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
    std::move(callback).Run(
        base::unexpected(ScreenshotResult::Status::kErrorServer));
    return;
  }

  client_->UploadEncryptedScreenshot(
      std::move(screenshot_ptr),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              [](GlicMetricReporter reporter,
                 base::OnceCallback<void(
                     base::expected<std::string, ScreenshotResult::Status>)>
                     callback,
                 const std::optional<std::string>& file_token) {
                bool success = file_token && !file_token->empty() &&
                               file_token->size() <= kMaxFileTokenBytes;
                reporter.RecordBoolean("Actor.EncryptedScreenshotUploadSuccess",
                                       success);
                if (!success) {
                  if (!file_token) {
                    DLOG(WARNING)
                        << "Glic Web Client failed to upload encrypted "
                           "screenshot (null token returned or callback "
                           "dropped).";
                  } else if (file_token->empty()) {
                    DLOG(WARNING)
                        << "Glic Web Client returned an empty file token upon "
                           "screenshot upload.";
                  } else if (file_token->size() > kMaxFileTokenBytes) {
                    DLOG(ERROR)
                        << "Excessively long file token received from Glic Web "
                           "Client: "
                        << file_token->size() << " bytes (max "
                        << kMaxFileTokenBytes << ").";
                  }
                  std::move(callback).Run(
                      base::unexpected(ScreenshotResult::Status::kErrorServer));
                  return;
                }
                std::move(callback).Run(base::ok(*file_token));
              },
              reporter, std::move(callback)),
          std::nullopt));
}

}  // namespace glic
