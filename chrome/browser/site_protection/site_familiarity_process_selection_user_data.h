// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_PROCESS_SELECTION_USER_DATA_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_PROCESS_SELECTION_USER_DATA_H_

#include "content/public/browser/process_selection_user_data.h"

namespace site_protection {

// ProcessSelectionUserData which stores an approximation of whether the site is
// familiar to the user. The site familiarity computation is based on the user's
// browsing history stored in chrome://history and the safe-browsing
// high-confidence-allowlist.
class SiteFamiliarityProcessSelectionUserData
    : public content::ProcessSelectionUserData::Data<
          SiteFamiliarityProcessSelectionUserData> {
 public:
  PROCESS_SELECTION_USER_DATA_KEY_DECL();

  explicit SiteFamiliarityProcessSelectionUserData(bool is_site_familiar);
  ~SiteFamiliarityProcessSelectionUserData() override;

  bool is_site_familiar() const { return is_site_familiar_; }

 private:
  bool is_site_familiar_;
};

}  // namespace site_protection

#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_PROCESS_SELECTION_USER_DATA_H_
