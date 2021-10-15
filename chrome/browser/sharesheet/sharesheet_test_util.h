// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_TEST_UTIL_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_TEST_UTIL_H_

#include "components/services/app_service/public/mojom/types.mojom.h"

namespace sharesheet {

const char kTestUrl[] = "https://fake-url.com/fake";

apps::mojom::IntentPtr CreateValidTextIntent();

apps::mojom::IntentPtr CreateInvalidIntent();

apps::mojom::IntentPtr CreateDriveIntent();

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_TEST_UTIL_H_
