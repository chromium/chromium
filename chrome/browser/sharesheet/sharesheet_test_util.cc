// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_test_util.h"

#include "components/services/app_service/public/cpp/intent_util.h"

namespace sharesheet {

apps::mojom::IntentPtr CreateValidTextIntent() {
  return apps_util::CreateShareIntentFromText("text", "title");
}

apps::mojom::IntentPtr CreateInvalidIntent() {
  auto intent = apps::mojom::Intent::New();
  intent->action = apps_util::kIntentActionSend;
  return intent;
}

apps::mojom::IntentPtr CreateDriveIntent() {
  return apps_util::CreateShareIntentFromDriveFile(GURL(kTestUrl), "image/",
                                                   GURL(kTestUrl), false);
}

}  // namespace sharesheet
