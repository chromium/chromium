// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_MOCK_CHROME_COMPOSE_CLIENT_H_
#define CHROME_BROWSER_COMPOSE_MOCK_CHROME_COMPOSE_CLIENT_H_

#include "chrome/browser/compose/chrome_compose_client.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class MockChromeComposeClient : public ChromeComposeClient {
 public:
  explicit MockChromeComposeClient(content::WebContents* web_contents);
  ~MockChromeComposeClient() override;

  MOCK_METHOD(bool,
              ShouldTriggerContextMenu,
              (content::RenderFrameHost*, content::ContextMenuParams&),
              (override));
};

#endif  // CHROME_BROWSER_COMPOSE_MOCK_CHROME_COMPOSE_CLIENT_H_
