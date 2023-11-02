// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONSENT_AUDITOR_CONSENT_AUDITOR_TEST_UTILS_H_
#define CHROME_BROWSER_CONSENT_AUDITOR_CONSENT_AUDITOR_TEST_UTILS_H_

#include <memory>

namespace content {
class BrowserContext;
}

class KeyedService;

std::unique_ptr<KeyedService> BuildFakeConsentAuditor(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_CONSENT_AUDITOR_CONSENT_AUDITOR_TEST_UTILS_H_
