// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/btm/stateful_bounce_counter.h"

#include <memory>

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/btm_service.h"
#include "content/public/browser/web_contents_user_data.h"

namespace btm {

StatefulBounceCounter::StatefulBounceCounter(PassKey,
                                             content::BtmService* dips_service)
    : dips_service_(dips_service) {
  dips_service_->AddObserver(this);
}

StatefulBounceCounter::~StatefulBounceCounter() {
  dips_service_->RemoveObserver(this);
}

/*static*/
void StatefulBounceCounter::CreateFor(content::BtmService* dips_service) {
  CHECK(!dips_service->GetUserData(&kUserDataKey));
  dips_service->SetUserData(
      &kUserDataKey,
      std::make_unique<StatefulBounceCounter>(PassKey(), dips_service));
}

void StatefulBounceCounter::OnStatefulBounce(
    content::WebContents* web_contents) {
  if (auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
          web_contents->GetPrimaryPage())) {
    pscs->IncrementStatefulBounceCount();
  }
}

const int StatefulBounceCounter::kUserDataKey;

}  // namespace btm
