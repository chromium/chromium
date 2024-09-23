// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pdf_blob_data_flattener.h"

#include <cstring>
#include <utility>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/printing/public/mojom/printing_service.mojom.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/blob_reader.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "printing/metafile_skia.h"
#include "printing/printing_utils.h"

namespace printing {

namespace {
bool g_disable_pdf_flattening_for_testing = false;
}  // namespace

FlattenPdfResult::FlattenPdfResult(
    std::unique_ptr<MetafileSkia> flattened_pdf_in,
    uint32_t page_count)
    : flattened_pdf(std::move(flattened_pdf_in)), page_count(page_count) {
  CHECK_GT(page_count, 0U);
  CHECK(flattened_pdf);
}

FlattenPdfResult::~FlattenPdfResult() = default;

PdfBlobDataFlattener::PdfBlobDataFlattener(Profile* profile)
    : profile_(*profile) {}

PdfBlobDataFlattener::~PdfBlobDataFlattener() = default;

// static
base::AutoReset<bool> PdfBlobDataFlattener::DisablePdfFlatteningForTesting() {
  return base::AutoReset<bool>(&g_disable_pdf_flattening_for_testing, true);
}

void PdfBlobDataFlattener::ReadAndFlattenPdf(
    mojo::PendingRemote<blink::mojom::Blob> blob,
    ReadAndFlattenPdfCallback callback) {
  BlobReader::Read(
      std::move(blob),
      base::BindOnce(&PdfBlobDataFlattener::OnPdfRead,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PdfBlobDataFlattener::OnPdfRead(ReadAndFlattenPdfCallback callback,
                                     std::string data,
                                     int64_t /*blob_total_size*/) {
  if (!LooksLikePdf(base::as_bytes(base::make_span(data)))) {
    std::move(callback).Run(/*result=*/nullptr);
    return;
  }

  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(data.size());
  if (!memory.IsValid()) {
    std::move(callback).Run(/*result=*/nullptr);
    return;
  }
  memcpy(memory.mapping.memory(), data.data(), data.size());

  if (g_disable_pdf_flattening_for_testing) {
    CHECK_IS_TEST();
    OnPdfFlattened(std::move(callback),
                   mojom::FlattenPdfResult::New(std::move(memory.region),
                                                /*page_count=*/1));
    return;
  }

  if (!flattener_.is_bound()) {
    GetPrintingService()->BindPdfFlattener(
        flattener_.BindNewPipeAndPassReceiver());
    auto* prefs = profile_->GetPrefs();
    if (prefs &&
        prefs->IsManagedPreference(prefs::kPdfUseSkiaRendererEnabled)) {
      flattener_->SetUseSkiaRendererPolicy(
          prefs->GetBoolean(prefs::kPdfUseSkiaRendererEnabled));
    }
  }

  auto flatten_callback =
      base::BindOnce(&PdfBlobDataFlattener::OnPdfFlattened,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  flattener_->FlattenPdf(std::move(memory.region),
                         mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                             std::move(flatten_callback), nullptr));
}

void PdfBlobDataFlattener::OnPdfFlattened(ReadAndFlattenPdfCallback callback,
                                          mojom::FlattenPdfResultPtr result) {
  if (!result) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto mapping = result->flattened_pdf_region.Map();
  if (!mapping.IsValid()) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto metafile = std::make_unique<MetafileSkia>();
  CHECK(metafile->InitFromData(mapping.GetMemoryAsSpan<const uint8_t>()));
  std::move(callback).Run(std::make_unique<FlattenPdfResult>(
      std::move(metafile), result->page_count));
}

}  // namespace printing
