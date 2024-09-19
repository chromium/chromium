// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_STATEFUL_BOUNCE_COUNTER_H_
#define CHROME_BROWSER_DIPS_STATEFUL_BOUNCE_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/dips/dips_bounce_detector.h"
#include "chrome/browser/dips/dips_service.h"
#include "content/public/browser/web_contents_user_data.h"

namespace dips {

// This class exists just to call
// PageSpecificContentSettings::IncrementStatefulBounceCount() whenever the user
// is statefully bounced.
class StatefulBounceCounter : public DIPSWebContentsObserver::Observer,
                              public base::SupportsUserData::Data {
 public:
  using PassKey = base::PassKey<StatefulBounceCounter>;
  // The constructor takes a PassKey so only Get() can call std::make_unique().
  StatefulBounceCounter(PassKey, DIPSWebContentsObserver*);
  ~StatefulBounceCounter() override;

  // Get the instance for `dips_wco`, creating it if it doesn't exist yet.
  static StatefulBounceCounter* Get(DIPSWebContentsObserver* dips_wco);

  void OnStatefulBounce(content::WebContents*) override;

 private:
  raw_ptr<DIPSWebContentsObserver> dips_wco_;

  // For SupportsUserData:
  static const int kUserDataKey = 0;
};

}  // namespace dips

#endif  // CHROME_BROWSER_DIPS_STATEFUL_BOUNCE_COUNTER_H_
