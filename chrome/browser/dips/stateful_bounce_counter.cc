// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/stateful_bounce_counter.h"

#include <memory>

#include "chrome/browser/dips/dips_bounce_detector.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/web_contents_user_data.h"

namespace dips {

StatefulBounceCounter::StatefulBounceCounter(PassKey, DIPSService* dips_service)
    : dips_service_(dips_service) {
  dips_service_->AddObserver(this);
}

StatefulBounceCounter::~StatefulBounceCounter() {
  dips_service_->RemoveObserver(this);
}

/*static*/
void StatefulBounceCounter::CreateFor(DIPSService* dips_service) {
  CHECK(!dips_service->GetUserData(&kUserDataKey));
  dips_service->SetUserData(
      &kUserDataKey,
      std::make_unique<StatefulBounceCounter>(PassKey(), dips_service));
}

void StatefulBounceCounter::OnStatefulBounce(
    content::WebContents* web_contents) {
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      web_contents->GetPrimaryPage());
  pscs->IncrementStatefulBounceCount();
}

const int StatefulBounceCounter::kUserDataKey;

}  // namespace dips
