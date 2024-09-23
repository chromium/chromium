// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/stateful_bounce_counter.h"

#include <memory>

#include "chrome/browser/dips/dips_bounce_detector.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/web_contents_user_data.h"

namespace dips {

StatefulBounceCounter::StatefulBounceCounter(PassKey,
                                             DIPSWebContentsObserver* dips_wco)
    : dips_wco_(dips_wco) {
  dips_wco_->AddObserver(this);
}

StatefulBounceCounter::~StatefulBounceCounter() {
  dips_wco_->RemoveObserver(this);
}

/*static*/
StatefulBounceCounter* StatefulBounceCounter::Get(
    DIPSWebContentsObserver* dips_wco) {
  if (void* data = dips_wco->GetUserData(&kUserDataKey)) {
    return static_cast<StatefulBounceCounter*>(data);
  }
  auto counter = std::make_unique<StatefulBounceCounter>(PassKey(), dips_wco);
  StatefulBounceCounter* p = counter.get();  // grab a pointer before moving it.
  dips_wco->SetUserData(&kUserDataKey, std::move(counter));
  return p;
}

void StatefulBounceCounter::OnStatefulBounce(
    content::WebContents* web_contents) {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      web_contents->GetPrimaryPage());
  pscs->IncrementStatefulBounceCount();
}

const int StatefulBounceCounter::kUserDataKey;

}  // namespace dips
