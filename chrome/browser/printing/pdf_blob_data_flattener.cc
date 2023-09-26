// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/pdf_blob_data_flattener.h"

#include <cstring>
#include <utility>

#include "base/check_is_test.h"
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
                                     std::unique_ptr<std::string> data,
                                     int64_t /*blob_total_size*/) {
  if (!data || !LooksLikePdf(*data)) {
    std::move(callback).Run(/*flattened_pdf=*/nullptr);
    return;
  }

  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(data->size());
  if (!memory.IsValid()) {
    std::move(callback).Run(/*flattened_pdf=*/nullptr);
    return;
  }
  memcpy(memory.mapping.memory(), data->data(), data->size());

  if (g_disable_pdf_flattening_for_testing) {
    CHECK_IS_TEST();
    OnPdfFlattened(std::move(callback), std::move(memory.region));
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
  flattener_->FlattenPdf(
      std::move(memory.region),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(flatten_callback), base::ReadOnlySharedMemoryRegion()));
}

void PdfBlobDataFlattener::OnPdfFlattened(
    ReadAndFlattenPdfCallback callback,
    base::ReadOnlySharedMemoryRegion flattened_pdf) {
  auto mapping = flattened_pdf.Map();
  if (!mapping.IsValid()) {
    std::move(callback).Run(/*flattened_pdf=*/nullptr);
    return;
  }
  auto metafile = std::make_unique<MetafileSkia>();
  CHECK(metafile->InitFromData(mapping.GetMemoryAsSpan<const uint8_t>()));
  std::move(callback).Run(std::move(metafile));
}

}  // namespace printing
