// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_STATEFUL_BOUNCE_COUNTER_H_
#define CHROME_BROWSER_DIPS_STATEFUL_BOUNCE_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/dips/dips_service.h"

namespace dips {

// This class exists just to call
// PageSpecificContentSettings::IncrementStatefulBounceCount() whenever the user
// is statefully bounced.
class StatefulBounceCounter : public DIPSService::Observer,
                              public base::SupportsUserData::Data {
 public:
  using PassKey = base::PassKey<StatefulBounceCounter>;

  // The constructor takes a PassKey so only CreateFor() can call
  // std::make_unique<StatefulBounceCounter>().
  StatefulBounceCounter(PassKey, DIPSService*);
  ~StatefulBounceCounter() override;

  // Create a StatefulBounceCounter that observes `dips_service` and will be
  // destroyed automatically.
  static void CreateFor(DIPSService* dips_service);

  void OnStatefulBounce(content::WebContents*) override;

 private:
  raw_ptr<DIPSService> dips_service_;

  // For SupportsUserData:
  static const int kUserDataKey = 0;
};

}  // namespace dips

#endif  // CHROME_BROWSER_DIPS_STATEFUL_BOUNCE_COUNTER_H_
