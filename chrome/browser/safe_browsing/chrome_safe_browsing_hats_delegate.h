// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_HATS_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_HATS_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"

namespace safe_browsing {

class ChromeSafeBrowsingHatsDelegate : public SafeBrowsingHatsDelegate {
 public:
  ChromeSafeBrowsingHatsDelegate();
  explicit ChromeSafeBrowsingHatsDelegate(HatsService* hats_service);

  void LaunchRedWarningSurvey(const std::map<std::string, std::string>&
                                  product_specific_string_data = {}) override;

 private:
  raw_ptr<HatsService> hats_service_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_SAFE_BROWSING_HATS_DELEGATE_H_
