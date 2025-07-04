// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_REQUEST_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "components/permissions/permission_request.h"

namespace url {
class Origin;
}

class SmartCardPermissionRequest : public permissions::PermissionRequest {
 public:
  using ResultCallback = base::OnceCallback<void(PermissionDecision)>;

  SmartCardPermissionRequest(const url::Origin& requesting_origin,
                             const std::string& reader_name,
                             ResultCallback result_callback);
  ~SmartCardPermissionRequest() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SmartCardPermissionRequestTest, IsDuplicateOf);
  FRIEND_TEST_ALL_PREFIXES(SmartCardPermissionRequestTest,
                           IsDuplicateOf_DifferentReader);
  FRIEND_TEST_ALL_PREFIXES(SmartCardPermissionRequestTest,
                           IsDuplicateOf_DifferentOrigin);

  // permissions::PermissionRequest:
  bool IsDuplicateOf(
      permissions::PermissionRequest* other_request) const override;
  std::u16string GetMessageTextFragment() const override;
  std::optional<std::u16string> GetAllowAlwaysText() const override;
  std::optional<std::u16string> GetBlockText() const override;

  void OnPermissionDecided(
      PermissionDecision decision,
      bool is_final_decision,
      const permissions::PermissionRequestData& request_data);

  std::string reader_name_;
  ResultCallback result_callback_;
};

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_REQUEST_H_
