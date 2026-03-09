// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/web_contents/mahi_content_extraction_delegate.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chromeos/components/mahi/public/cpp/mahi_browser_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_types.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/accessibility/ax_tree_update.h"
#include "url/gurl.h"

namespace mahi {

MahiContentExtractionDelegate::MahiContentExtractionDelegate() {
  // Do not bind to the services if mahi is not enabled.
  if (!chromeos::features::IsMahiEnabled()) {
    return;
  }

  // Builds connection with mahi content extraction service.
  EnsureContentExtractionServiceIsSetUp();
  EnsureServiceIsConnected();

  // Builds connection with screen ai service.
  screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
      ProfileManager::GetActiveUserProfile())
      ->GetServiceStateAsync(
          screen_ai::ScreenAIServiceRouter::Service::kMainContentExtraction,
          base::BindOnce(
              &MahiContentExtractionDelegate::OnScreenAIServiceInitialized,
              weak_pointer_factory_.GetWeakPtr()));
}

MahiContentExtractionDelegate::~MahiContentExtractionDelegate() = default;

bool MahiContentExtractionDelegate::EnsureContentExtractionServiceIsSetUp() {
  if (remote_content_extraction_service_factory_ &&
      remote_content_extraction_service_factory_.is_bound()) {
    return true;
  }

  content::ServiceProcessHost::Launch(
      remote_content_extraction_service_factory_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Mahi Content Extraction Service")
          .Pass());

  remote_content_extraction_service_factory_.reset_on_disconnect();

  return remote_content_extraction_service_factory_ &&
         remote_content_extraction_service_factory_.is_bound();
}

bool MahiContentExtractionDelegate::EnsureServiceIsConnected() {
  if (remote_content_extraction_service_ &&
      remote_content_extraction_service_.is_bound()) {
    return true;
  }

  remote_content_extraction_service_factory_->BindContentExtractionService(
      remote_content_extraction_service_.BindNewPipeAndPassReceiver());
  remote_content_extraction_service_.reset_on_disconnect();

  return remote_content_extraction_service_ &&
         remote_content_extraction_service_.is_bound();
}

void MahiContentExtractionDelegate::ExtractContent(
    const WebContentState& web_content_state,
    const base::UnguessableToken& client_id,
    chromeos::MahiGetContentCallback callback) {
  // Early returns if the snapshot is not valid.
  if (web_content_state.snapshot.root_id == ui::kInvalidAXNodeID) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Generates the extraction request.
  mojom::ExtractionRequestPtr extraction_request =
      mojom::ExtractionRequest::New(
          /*deprecated_ukm_source_id=*/ukm::kInvalidSourceId,
          /*snapshot=*/std::make_optional(web_content_state.snapshot),
          /*extraction_methods=*/
          mojom::ExtractionMethods::New(/*use_algorithm=*/true,
                                        /*use_screen2x=*/true),
          /*updates=*/std::nullopt);

  if (!EnsureContentExtractionServiceIsSetUp() || !EnsureServiceIsConnected()) {
    std::move(callback).Run(std::nullopt);
    LOG(ERROR) << "Remote content extraction service is not available.";
    return;
  }
  MaybeBindScreenAIContentExtraction();

  remote_content_extraction_service_->ExtractContent(
      std::move(extraction_request),
      base::BindOnce(&MahiContentExtractionDelegate::OnGetContent,
                     weak_pointer_factory_.GetWeakPtr(),
                     web_content_state.page_id, client_id,
                     web_content_state.url, std::move(callback)));
}

void MahiContentExtractionDelegate::ExtractContent(
    const WebContentState& web_content_state,
    const std::vector<ui::AXTreeUpdate>& updates,
    const base::UnguessableToken& client_id,
    chromeos::MahiGetContentCallback callback) {
  // Generates the extraction request.
  mojom::ExtractionRequestPtr extraction_request =
      mojom::ExtractionRequest::New(
          /*deprecated_ukm_source_id=*/ukm::kInvalidSourceId,
          /*snapshot=*/std::nullopt,
          /*extraction_methods=*/
          mojom::ExtractionMethods::New(/*use_algorithm=*/true,
                                        /*use_screen2x=*/true),
          /*updates=*/std::make_optional(updates));

  if (!EnsureContentExtractionServiceIsSetUp() || !EnsureServiceIsConnected()) {
    std::move(callback).Run(std::nullopt);
    LOG(ERROR) << "Remote content extraction service is not available.";
    return;
  }
  MaybeBindScreenAIContentExtraction();

  remote_content_extraction_service_->ExtractContent(
      std::move(extraction_request),
      base::BindOnce(&MahiContentExtractionDelegate::OnGetContent,
                     weak_pointer_factory_.GetWeakPtr(),
                     web_content_state.page_id, client_id,
                     web_content_state.url, std::move(callback)));
}

void MahiContentExtractionDelegate::OnGetContent(
    const base::UnguessableToken& page_id,
    const base::UnguessableToken& client_id,
    const GURL& url,
    chromeos::MahiGetContentCallback callback,
    mojom::ExtractionResponsePtr response) {
  chromeos::MahiPageContent page_content;
  page_content.client_id = client_id;
  page_content.page_id = page_id;
  page_content.page_content = std::move(response->contents);

  std::move(callback).Run(std::move(page_content));
}

void MahiContentExtractionDelegate::OnScreenAIServiceInitialized(
    bool successful) {
  screen_ai_service_initialized_ = successful;
  if (!successful) {
    LOG(ERROR) << "ScreenAI service was unsuccessfuly initialized.";
    return;
  }

  MaybeBindScreenAIContentExtraction();
}

void MahiContentExtractionDelegate::MaybeBindScreenAIContentExtraction() {
  // Screen AI service isn't initialize yet.
  if (!screen_ai_service_initialized_) {
    return;
  }

  if (!EnsureContentExtractionServiceIsSetUp()) {
    LOG(ERROR) << "Content extraction service isn't available.";
    return;
  }

  mojo::PendingReceiver<screen_ai::mojom::Screen2xMainContentExtractor>
      screen_ai_receiver;
  auto screen_ai_remote = screen_ai_receiver.InitWithNewPipeAndPassRemote();

  screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
      ProfileManager::GetActiveUserProfile())
      ->BindMainContentExtractor(std::move(screen_ai_receiver));
  remote_content_extraction_service_factory_->OnScreen2xReady(
      std::move(screen_ai_remote));
}

}  // namespace mahi
