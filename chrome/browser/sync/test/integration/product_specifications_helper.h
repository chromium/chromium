// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRODUCT_SPECIFICATIONS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRODUCT_SPECIFICATIONS_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"

namespace sync_pb {
class ProductComparisonSpecifics;
}  // namespace sync_pb

namespace commerce {

class ProductSpecificationsService;

// Class that checks when CompareSpecifics are available to be used
// by ProductSpecificationsService.
class ProductSpecificationsChecker : public StatusChangeChecker {
 public:
  ProductSpecificationsChecker(
      commerce::ProductSpecificationsService* service,
      const sync_pb::ProductComparisonSpecifics* product_comparison_specifics);

  ProductSpecificationsChecker(const ProductSpecificationsChecker&) = delete;
  ProductSpecificationsChecker& operator=(const ProductSpecificationsChecker&) =
      delete;

  ~ProductSpecificationsChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const raw_ptr<const sync_pb::ProductComparisonSpecifics>
      product_comparison_specifics_;
  const raw_ptr<commerce::ProductSpecificationsService> service_;

  bool IsSpecificsAvailableAndEqual();
};

}  // namespace commerce

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PRODUCT_SPECIFICATIONS_HELPER_H_
