// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_GCA_SERVICE_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_GCA_SERVICE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/devtools_http_service_handler.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GcaServiceHandler : public DevToolsHttpServiceHandler {
 public:
  static const net::NetworkTrafficAnnotationTag& TrafficAnnotation();

  GcaServiceHandler();
  ~GcaServiceHandler() override;

 private:
  // DevToolsHttpServiceHandler overrides:
  void CanMakeRequest(Profile* profile,
                      base::OnceCallback<void(bool success)> callback) override;
  GURL BaseURL() const override;
  signin::OAuthConsumerId OAuthConsumerId() const override;
  net::NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag() const override;
};

#endif  // CHROME_BROWSER_DEVTOOLS_GCA_SERVICE_HANDLER_H_
