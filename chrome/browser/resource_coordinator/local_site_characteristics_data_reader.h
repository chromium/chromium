// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_READER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_READER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_reader.h"

namespace resource_coordinator {

FORWARD_DECLARE_TEST(LocalSiteCharacteristicsDataReaderTest,
                     FreeingReaderDoesntCauseWriteOperation);

namespace internal {
class LocalSiteCharacteristicsDataImpl;
}  // namespace internal

// Specialization of a SiteCharacteristicDataReader that delegates to a
// LocalSiteCharacteristicsDataImpl.
class LocalSiteCharacteristicsDataReader
    : public SiteCharacteristicsDataReader {
 public:
  ~LocalSiteCharacteristicsDataReader() override;

  // SiteCharacteristicsDataReader:
  performance_manager::SiteFeatureUsage UpdatesFaviconInBackground()
      const override;
  performance_manager::SiteFeatureUsage UpdatesTitleInBackground()
      const override;
  performance_manager::SiteFeatureUsage UsesAudioInBackground() const override;
  performance_manager::SiteFeatureUsage UsesNotificationsInBackground()
      const override;
  bool DataLoaded() const override;
  void RegisterDataLoadedCallback(base::OnceClosure&& callback) override;

  internal::LocalSiteCharacteristicsDataImpl* impl_for_testing() const {
    return impl_.get();
  }

 private:
  friend class LocalSiteCharacteristicsDataReaderTest;
  friend class LocalSiteCharacteristicsDataStoreTest;
  friend class LocalSiteCharacteristicsDataStore;
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataReaderTest,
                           DestroyingReaderCancelsPendingCallbacks);
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataReaderTest,
                           FreeingReaderDoesntCauseWriteOperation);
  FRIEND_TEST_ALL_PREFIXES(LocalSiteCharacteristicsDataReaderTest,
                           OnDataLoadedCallbackInvoked);

  // Private constructor, these objects are meant to be created by a site
  // characteristics data store.
  explicit LocalSiteCharacteristicsDataReader(
      scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl);

  // Runs the provided closure. This is used as a wrapper so that callbacks
  // registered with the |impl_| by this reader are invalidated when the
  // reader is destroyed.
  void RunClosure(base::OnceClosure&& closure);

  // The LocalSiteCharacteristicDataInternal object we delegate to.
  const scoped_refptr<internal::LocalSiteCharacteristicsDataImpl> impl_;

  // Used for invalidating callbacks.
  base::WeakPtrFactory<LocalSiteCharacteristicsDataReader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataReader);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_READER_H_
