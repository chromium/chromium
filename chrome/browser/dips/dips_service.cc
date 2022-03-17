// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include "chrome/browser/dips/dips_service_factory.h"

namespace dips {

DIPSService::DIPSService(content::BrowserContext* context)
    : browser_context_(context) {}

DIPSService::~DIPSService() = default;

/* static */
DIPSService* DIPSService::Get(content::BrowserContext* context) {
  return DIPSServiceFactory::GetForBrowserContext(context);
}

}  // namespace dips
