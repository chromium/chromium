// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_AIDA_SERVICE_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_AIDA_SERVICE_HANDLER_H_

#include "chrome/browser/devtools/devtools_http_service_handler.h"

class AidaServiceHandler : public DevToolsHttpServiceHandler {
 public:
  static const net::NetworkTrafficAnnotationTag& TrafficAnnotation();

  AidaServiceHandler();
  ~AidaServiceHandler() override;

 private:
  // DevToolsHttpServiceHandler overrides:
  void CanMakeRequest(Profile* profile,
                      base::OnceCallback<void(bool success)> callback) override;
  GURL BaseURL() const override;
  signin::ScopeSet OAuthScopes() const override;
  net::NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag() const override;
};

#endif  // CHROME_BROWSER_DEVTOOLS_AIDA_SERVICE_HANDLER_H_
