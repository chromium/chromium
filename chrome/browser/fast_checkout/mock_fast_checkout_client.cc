// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/mock_fast_checkout_client.h"

MockFastCheckoutClient::MockFastCheckoutClient(
    content::WebContents* web_contents)
    : FastCheckoutClientImpl(web_contents) {}

MockFastCheckoutClient::~MockFastCheckoutClient() = default;
