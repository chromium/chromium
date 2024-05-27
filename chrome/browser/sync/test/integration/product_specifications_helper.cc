// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/product_specifications_helper.h"

#include <sstream>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"

namespace commerce {

ProductSpecificationsChecker::ProductSpecificationsChecker(
    commerce::ProductSpecificationsService* service,
    const sync_pb::ProductComparisonSpecifics* product_comparison_specifics)
    : product_comparison_specifics_(product_comparison_specifics),
      service_(service) {}

ProductSpecificationsChecker::~ProductSpecificationsChecker() = default;

bool ProductSpecificationsChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for CompareSpecifics:\n";
  *os << "    uuid: " << product_comparison_specifics_->uuid() << "\n";
  *os << "    name: " << product_comparison_specifics_->name() << "\n";
  *os << "    creation_time: "
      << product_comparison_specifics_->creation_time_unix_epoch_micros()
      << "\n";
  *os << "    update_time: "
      << product_comparison_specifics_->update_time_unix_epoch_micros() << "\n";
  std::vector<std::string> urls;
  for (const sync_pb::ComparisonData& comparison_data :
       product_comparison_specifics_->data()) {
    urls.push_back(comparison_data.url());
  }
  *os << "    urls: " << base::JoinString(urls, ", ");
  return IsSpecificsAvailableAndEqual();
}

bool ProductSpecificationsChecker::IsSpecificsAvailableAndEqual() {
  for (const ProductSpecificationsSet& product_specifications_set :
       service_->GetAllProductSpecifications()) {
    std::vector<GURL> specifics_urls;
    for (sync_pb::ComparisonData data : product_comparison_specifics_->data()) {
      specifics_urls.emplace_back(data.url());
    }
    if (product_specifications_set.uuid().AsLowercaseString() ==
            product_comparison_specifics_->uuid() &&
        product_specifications_set.name() ==
            product_comparison_specifics_->name() &&
        product_specifications_set.creation_time() ==
            base::Time::FromMillisecondsSinceUnixEpoch(
                product_comparison_specifics_
                    ->creation_time_unix_epoch_micros()) &&
        product_specifications_set.update_time() ==
            base::Time::FromMillisecondsSinceUnixEpoch(
                product_comparison_specifics_
                    ->update_time_unix_epoch_micros()) &&
        product_specifications_set.urls() == specifics_urls) {
      return true;
    }
  }
  return false;
}

}  // namespace commerce
