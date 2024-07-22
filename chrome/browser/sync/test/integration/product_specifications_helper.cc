// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/product_specifications_helper.h"

#include <sstream>

#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/sync/protocol/product_comparison_specifics.pb.h"

namespace commerce {

ProductSpecificationsChecker::ProductSpecificationsChecker(
    ProductSpecificationsService* service,
    ProductSpecificationsSet* expected_set)
    : expected_set_(expected_set), service_(service) {}

ProductSpecificationsChecker::~ProductSpecificationsChecker() = default;

bool ProductSpecificationsChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for ProductSpeificationsSet:\n";
  *os << "    uuid: " << expected_set_->uuid() << "\n";
  *os << "    name: " << expected_set_->name() << "\n";
  *os << "    creation_time: " << expected_set_->creation_time() << "\n";
  *os << "    update_time: " << expected_set_->update_time() << "\n";
  *os << "    urls: "
      << base::JoinString(base::ToVector(expected_set_->urls(), &GURL::spec),
                          ", ")
      << "\n";
  return IsSpecificsAvailableAndEqual();
}

bool ProductSpecificationsChecker::IsSpecificsAvailableAndEqual() {
  for (const ProductSpecificationsSet& product_specifications_set :
       service_->GetAllProductSpecifications()) {
    // TODO(crbug.com/354689571) create == operator on
    // ProductSpecificationsSet.
    if (product_specifications_set.uuid() == expected_set_->uuid() &&
        product_specifications_set.name() == expected_set_->name() &&
        product_specifications_set.creation_time() ==
            expected_set_->creation_time() &&
        product_specifications_set.update_time() ==
            expected_set_->update_time() &&
        product_specifications_set.urls() == expected_set_->urls()) {
      return true;
    }
  }
  return false;
}

}  // namespace commerce
