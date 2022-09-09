// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOMAIN_RELIABILITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOMAIN_RELIABILITY_SERVICE_FACTORY_H_

namespace domain_reliability {

class DomainReliabilityServiceFactory {
 public:
  static bool ShouldCreateService();

  static const char kUploadReporterString[];
};

}  // namespace domain_reliability

#endif  // CHROME_BROWSER_DOMAIN_RELIABILITY_SERVICE_FACTORY_H_
