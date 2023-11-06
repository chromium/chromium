// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/mock_chrome_compose_client.h"

MockChromeComposeClient::MockChromeComposeClient(
    content::WebContents* web_contents)
    : ChromeComposeClient(web_contents) {}

MockChromeComposeClient::~MockChromeComposeClient() = default;
