// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_content_extraction_delegate.h"

#include <algorithm>

#include "base/functional/callback.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace mahi {

namespace {
// The word count threshold for a distillable page.
static constexpr int kWordCountThreshold = 500;
}  // namespace

MahiContentExtractionDelegate::MahiContentExtractionDelegate(
    base::RepeatingCallback<void(const base::UnguessableToken&, bool)>
        distillable_check_callback)
    : distillable_check_callback_(std::move(distillable_check_callback)) {
  if (chromeos::features::IsMahiEnabled()) {
    SetUpContentExtractionService();
    EnsureServiceIsConnected();
  }
}

MahiContentExtractionDelegate::~MahiContentExtractionDelegate() = default;

bool MahiContentExtractionDelegate::SetUpContentExtractionService() {
  if (remote_content_extraction_service_factory_ &&
      remote_content_extraction_service_factory_.is_bound()) {
    return false;
  }

  content::ServiceProcessHost::Launch(
      remote_content_extraction_service_factory_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Mahi Content Extraction Service")
          .Pass());

  remote_content_extraction_service_factory_.reset_on_disconnect();

  return true;
}

void MahiContentExtractionDelegate::EnsureServiceIsConnected() {
  if (remote_content_extraction_service_ &&
      remote_content_extraction_service_.is_bound()) {
    return;
  }

  remote_content_extraction_service_factory_->BindContentExtractionService(
      remote_content_extraction_service_.BindNewPipeAndPassReceiver());
  remote_content_extraction_service_.reset_on_disconnect();
}

void MahiContentExtractionDelegate::ExtractContent(
    const WebContentState& web_content_state,
    const base::UnguessableToken& client_id,
    GetContentCallback callback) {
  // Early returns if the snapshot is not valid.
  if (web_content_state.snapshot.root_id == ui::kInvalidAXNodeID) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Both algorithms are used for content extraction.
  mojom::ExtractionMethodsPtr extraction_methods =
      mojom::ExtractionMethods::New(/*use_algorithm=*/true,
                                    /*use_screen2x=*/true);

  // Generates the extraction request.
  mojom::ExtractionRequestPtr extraction_request =
      mojom::ExtractionRequest::New(
          /*ukm_source_id=*/web_content_state.ukm_source_id,
          /*snapshot=*/web_content_state.snapshot,
          /*extraction_methods=*/std::move(extraction_methods));

  remote_content_extraction_service_->ExtractContent(
      std::move(extraction_request),
      base::BindOnce(&MahiContentExtractionDelegate::OnGetContent,
                     weak_pointer_factory_.GetWeakPtr(),
                     web_content_state.page_id, client_id,
                     std::move(callback)));
}

void MahiContentExtractionDelegate::CheckDistillablity(
    const WebContentState& web_content_state) {
  // Early returns if the snapshot is not valid.
  // TODO(b/318565573) consider adding some error states so that OS side have a
  // better sense of the operations on the browser side.
  if (web_content_state.snapshot.root_id == ui::kInvalidAXNodeID) {
    return;
  }

  // Only rule based algorithm is used for triggering check.
  mojom::ExtractionMethodsPtr extraction_methods =
      mojom::ExtractionMethods::New(/*use_algorithm=*/true,
                                    /*use_screen2x=*/false);

  // Generates the extraction request.
  mojom::ExtractionRequestPtr extraction_request =
      mojom::ExtractionRequest::New(
          /*ukm_source_id=*/web_content_state.ukm_source_id,
          /*snapshot=*/web_content_state.snapshot,
          /*extraction_methods=*/std::move(extraction_methods));

  remote_content_extraction_service_->GetContentSize(
      std::move(extraction_request),
      base::BindOnce(&MahiContentExtractionDelegate::OnGetContentSize,
                     weak_pointer_factory_.GetWeakPtr(),
                     web_content_state.page_id));
}

void MahiContentExtractionDelegate::OnGetContentSize(
    const base::UnguessableToken& page_id,
    mojom::ContentSizeResponsePtr response) {
  distillable_check_callback_.Run(page_id,
                                  response->word_count >= kWordCountThreshold);
}

void MahiContentExtractionDelegate::OnGetContent(
    const base::UnguessableToken& page_id,
    const base::UnguessableToken& client_id,
    GetContentCallback callback,
    mojom::ExtractionResponsePtr response) {
  crosapi::mojom::MahiPageContentPtr page_content =
      crosapi::mojom::MahiPageContent::New(
          /*client_id=*/client_id,
          /*page_id=*/page_id,
          /*page_content=*/std::move(response->contents));

  std::move(callback).Run(std::move(page_content));
}

}  // namespace mahi
