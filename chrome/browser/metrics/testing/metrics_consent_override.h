// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TESTING_METRICS_CONSENT_OVERRIDE_H_
#define CHROME_BROWSER_METRICS_TESTING_METRICS_CONSENT_OVERRIDE_H_

namespace metrics::test {

// A helper object for overriding metrics enabled state.
class MetricsConsentOverride {
 public:
  explicit MetricsConsentOverride(bool initial_state);
  ~MetricsConsentOverride();

  void Update(bool state);

 private:
  bool state_;
};

}  // namespace metrics::test

#endif  // CHROME_BROWSER_METRICS_TESTING_METRICS_CONSENT_OVERRIDE_H_
