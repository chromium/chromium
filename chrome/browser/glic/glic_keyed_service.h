// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
#define CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

class GlicKeyedService : public KeyedService {
 public:
  explicit GlicKeyedService(content::BrowserContext* browser_context);
  GlicKeyedService(const GlicKeyedService&) = delete;
  GlicKeyedService& operator=(const GlicKeyedService&) = delete;
  ~GlicKeyedService() override;

  // Launches the Glic UI.
  void LaunchUI();

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

#endif  // CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
