// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_PROGRESS_OBSERVER_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_PROGRESS_OBSERVER_H_

#include "chrome/common/importer/importer_data_types.h"

namespace importer {

// Objects implement this interface when they wish to be notified of events
// during the import process.
class ImporterProgressObserver {
 public:
  // Invoked when the import begins.
  virtual void ImportStarted() = 0;

  // Invoked when data for the specified item is about to be collected.
  virtual void ImportItemStarted(ImportItem item) = 0;

  // Invoked when data for the specified item has been collected from the
  // source profile and is now ready for further processing.
  virtual void ImportItemEnded(ImportItem item) = 0;

  // Invoked when the source profile has been imported.
  virtual void ImportEnded() = 0;

 protected:
  virtual ~ImporterProgressObserver() {}
};

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_PROGRESS_OBSERVER_H_
