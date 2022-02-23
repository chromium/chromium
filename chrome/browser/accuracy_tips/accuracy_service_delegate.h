// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCURACY_TIPS_ACCURACY_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_ACCURACY_TIPS_ACCURACY_SERVICE_DELEGATE_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "components/accuracy_tips/accuracy_service.h"
#include "components/accuracy_tips/accuracy_tip_interaction.h"
#include "components/accuracy_tips/accuracy_tip_status.h"

namespace content {
class WebContents;
}

class Profile;

class AccuracyServiceDelegate
    : public accuracy_tips::AccuracyService::Delegate {
 public:
  explicit AccuracyServiceDelegate(Profile* profile);
  ~AccuracyServiceDelegate() override;

  AccuracyServiceDelegate(const AccuracyServiceDelegate&) = delete;
  AccuracyServiceDelegate& operator=(const AccuracyServiceDelegate&) = delete;

  bool IsEngagementHigh(const GURL& url) override;

  void ShowAccuracyTip(
      content::WebContents* web_contents,
      accuracy_tips::AccuracyTipStatus type,
      bool show_opt_out,
      base::OnceCallback<void(accuracy_tips::AccuracyTipInteraction)>
          close_callback) override;

  void ShowSurvey(const std::map<std::string, bool>& product_specific_bits_data,
                  const std::map<std::string, std::string>&
                      product_specific_string_data) override;

  bool IsSecureConnection(content::WebContents* web_contents) override;

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_ACCURACY_TIPS_ACCURACY_SERVICE_DELEGATE_H_
