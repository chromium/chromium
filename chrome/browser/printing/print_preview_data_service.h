// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DATA_SERVICE_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DATA_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"

class PrintPreviewDataStore;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
class RefCountedMemory;
}

// PrintPreviewDataService manages data stores for chrome://print requests.
// It owns the data store object and is responsible for freeing it.
class PrintPreviewDataService {
 public:
  static PrintPreviewDataService* GetInstance();

  PrintPreviewDataService(const PrintPreviewDataService&) = delete;
  PrintPreviewDataService& operator=(const PrintPreviewDataService&) = delete;

  // Gets the data entry from PrintPreviewDataStore. `index` is zero-based or
  // `printing::COMPLETE_PREVIEW_DOCUMENT_INDEX` to represent complete preview
  // data. Use `index` to retrieve a specific preview page data.
  // Returns nullptr if the requested page is not yet available.
  scoped_refptr<base::RefCountedMemory> GetDataEntry(int32_t preview_ui_id,
                                                     int index) const;

  // Sets/Updates the data entry in PrintPreviewDataStore. `index` is zero-based
  // or `printing::COMPLETE_PREVIEW_DOCUMENT_INDEX` to represent complete
  // preview data. Use `index` to set/update a specific preview page data.
  void SetDataEntry(int32_t preview_ui_id,
                    int index,
                    scoped_refptr<base::RefCountedMemory> data);

  // Removes the corresponding PrintPreviewUI entry from the map.
  void RemoveEntry(int32_t preview_ui_id);

 private:
  friend struct base::DefaultSingletonTraits<PrintPreviewDataService>;

  // 1:1 relationship between PrintPreviewUI and data store object.
  // Key: PrintPreviewUI ID.
  // Value: Print preview data store object.
  using PreviewDataStoreMap =
      std::map<int32_t, std::unique_ptr<PrintPreviewDataStore>>;

  PrintPreviewDataService();
  ~PrintPreviewDataService();

  PreviewDataStoreMap data_store_map_;
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DATA_SERVICE_H_
