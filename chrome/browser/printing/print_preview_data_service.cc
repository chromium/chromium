// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_data_service.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "printing/print_job_constants.h"
#include "printing/printing_utils.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/printing/xps_features.h"
#endif

// PrintPreviewDataStore stores data for preview workflow and preview printing
// workflow.
//
// NOTE:
//   This class stores a list of PDFs. The list `index` is zero-based and can
// be `printing::COMPLETE_PREVIEW_DOCUMENT_INDEX` to represent complete preview
// document. The PDF stored at `printing::COMPLETE_PREVIEW_DOCUMENT_INDEX` is
// optimized with font subsetting, compression, etc. PDF's stored at all other
// indices are unoptimized.
//
// PrintPreviewDataStore owns the data and is responsible for freeing it when
// either:
//    a) There is a new data.
//    b) When PrintPreviewDataStore is destroyed.
//
class PrintPreviewDataStore {
 public:
  PrintPreviewDataStore() {}

  PrintPreviewDataStore(const PrintPreviewDataStore&) = delete;
  PrintPreviewDataStore& operator=(const PrintPreviewDataStore&) = delete;

  ~PrintPreviewDataStore() {}

  // Get the preview page for the specified `index`.
  void GetPreviewDataForIndex(
      int index,
      scoped_refptr<base::RefCountedMemory>* data) const {
    if (IsInvalidIndex(index))
      return;

    auto it = page_data_map_.find(index);
    if (it != page_data_map_.end())
      *data = it->second.get();
  }

  // Set/Update the preview data entry for the specified `index`.
  void SetPreviewDataForIndex(int index,
                              scoped_refptr<base::RefCountedMemory> data) {
    if (IsInvalidIndex(index))
      return;

    DCHECK(data);
#if DCHECK_IS_ON()
    DCHECK(IsValidData(index, *data));
#endif

    page_data_map_[index] = std::move(data);
  }

 private:
  // 1:1 relationship between page index and its associated preview data.
  // Key: Page index is zero-based and can be
  // `printing::COMPLETE_PREVIEW_DOCUMENT_INDEX` to represent complete preview
  // document.
  // Value: Preview data.
  using PreviewPageDataMap =
      std::map<int, scoped_refptr<base::RefCountedMemory>>;

#if DCHECK_IS_ON()
  bool IsValidData(int index, base::span<const uint8_t> data) const {
#if BUILDFLAG(IS_WIN)
    // Do not have access here whether this print document is from a modifiable
    // source or not, so next best restriction is if some kind of XPS data
    // generation is to be expected.
    if (index == printing::COMPLETE_PREVIEW_DOCUMENT_INDEX &&
        printing::IsXpsPrintCapabilityRequired()) {
      // A valid Windows document could be PDF or XPS.
      printing::DocumentDataType data_type =
          printing::DetermineDocumentDataType(data);
      return data_type == printing::DocumentDataType::kPdf ||
             data_type == printing::DocumentDataType::kXps;
    }
#endif

    // Non-Windows and all individual pages are only ever supposed to be PDF.
    return printing::LooksLikePdf(data);
  }
#endif  // DCHECK_IS_ON()

  static bool IsInvalidIndex(int index) {
    return (index != printing::COMPLETE_PREVIEW_DOCUMENT_INDEX &&
            index < printing::FIRST_PAGE_INDEX);
  }

  PreviewPageDataMap page_data_map_;
};

// static
PrintPreviewDataService* PrintPreviewDataService::GetInstance() {
  return base::Singleton<PrintPreviewDataService>::get();
}

PrintPreviewDataService::PrintPreviewDataService() {
}

PrintPreviewDataService::~PrintPreviewDataService() {
}

void PrintPreviewDataService::GetDataEntry(
    int32_t preview_ui_id,
    int index,
    scoped_refptr<base::RefCountedMemory>* data_bytes) const {
  *data_bytes = nullptr;
  auto it = data_store_map_.find(preview_ui_id);
  if (it != data_store_map_.end())
    it->second->GetPreviewDataForIndex(index, data_bytes);
}

void PrintPreviewDataService::SetDataEntry(
    int32_t preview_ui_id,
    int index,
    scoped_refptr<base::RefCountedMemory> data_bytes) {
  if (!base::Contains(data_store_map_, preview_ui_id))
    data_store_map_[preview_ui_id] = std::make_unique<PrintPreviewDataStore>();
  data_store_map_[preview_ui_id]->SetPreviewDataForIndex(index,
                                                         std::move(data_bytes));
}

void PrintPreviewDataService::RemoveEntry(int32_t preview_ui_id) {
  data_store_map_.erase(preview_ui_id);
}
