// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_REQUEST_H_

#include "components/permissions/permission_request.h"

namespace url {
class Origin;
}

class SmartCardPermissionRequest : public permissions::PermissionRequest {
 public:
  enum class Result {
    kAllowOnce = 0,
    kAllowAlways = 1,
    kDontAllow = 2,
  };

  using ResultCallback = base::OnceCallback<void(Result)>;

  SmartCardPermissionRequest(const url::Origin& requesting_origin,
                             const std::string& reader_name,
                             ResultCallback result_callback);
  ~SmartCardPermissionRequest() override;

 private:
  // permissions::PermissionRequest:
  bool IsDuplicateOf(
      permissions::PermissionRequest* other_request) const override;
  std::u16string GetMessageTextFragment() const override;
  std::optional<std::u16string> GetAllowAlwaysText() const override;

  void OnPermissionDecided(ContentSetting result,
                           bool is_one_time,
                           bool is_final_decision);

  void DeleteRequest();

  std::string reader_name_;
  ResultCallback result_callback_;
};

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_PERMISSION_REQUEST_H_
