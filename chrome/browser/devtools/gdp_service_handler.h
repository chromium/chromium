// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_GDP_SERVICE_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_GDP_SERVICE_HANDLER_H_

#include "chrome/browser/devtools/devtools_http_service_handler.h"

class GdpServiceHandler : public DevToolsHttpServiceHandler {
 public:
  GdpServiceHandler();
  ~GdpServiceHandler() override;

 private:
  // DevToolsHttpServiceHandler overrides:
  GURL BaseURL() const override;
  signin::ScopeSet OAuthScopes() const override;
  net::NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag() const override;
};

#endif  // CHROME_BROWSER_DEVTOOLS_GDP_SERVICE_HANDLER_H_
