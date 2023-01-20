// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOMAIN_RELIABILITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOMAIN_RELIABILITY_SERVICE_FACTORY_H_

namespace domain_reliability {

// Determines if Domain Reliability service should be created based on
// command line flags, Chrome policies, and field trials.
// Used in //chrome/browser/net/profile_network_context_service.cc.
bool ShouldCreateService();

// Identifies Chrome as the source of Domain Reliability uploads it sends.
extern const char kUploadReporterString[];

}  // namespace domain_reliability

#endif  // CHROME_BROWSER_DOMAIN_RELIABILITY_SERVICE_FACTORY_H_
