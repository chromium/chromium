// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_preview_cros/backend/print_preview_pdf_store.h"

#include "base/check.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/unguessable_token.h"
#include "printing/printing_utils.h"

namespace ash::printing::print_preview {

PrintPreviewPdfStore::PrintPreviewPdfStore() {}
PrintPreviewPdfStore::~PrintPreviewPdfStore() {}

// static
PrintPreviewPdfStore* PrintPreviewPdfStore::GetInstance() {
  return base::Singleton<PrintPreviewPdfStore>::get();
}

void PrintPreviewPdfStore::InsertPdfData(
    const base::UnguessableToken& token,
    int page_index,
    scoped_refptr<base::RefCountedMemory> data) {
  CHECK(::printing::LooksLikePdf(*data));

  pdf_data_map_[token][page_index] = std::move(data);
}

scoped_refptr<base::RefCountedMemory> PrintPreviewPdfStore::GetPdfData(
    const base::UnguessableToken& token,
    int page_index) const {
  auto token_it = pdf_data_map_.find(token);
  if (token_it == pdf_data_map_.end()) {
    return nullptr;
  }

  auto page_index_it = token_it->second.find(page_index);
  if (page_index_it == token_it->second.end()) {
    return nullptr;
  }

  return page_index_it->second;
}

}  // namespace ash::printing::print_preview
