// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_CHROME_STRUCTURED_METRICS_DELEGATE_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_CHROME_STRUCTURED_METRICS_DELEGATE_H_

#include <memory>

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/prefs/pref_registry_simple.h"

namespace metrics::structured {

namespace {
using RecordingDelegate = StructuredMetricsClient::RecordingDelegate;
}  // namespace

// Singleton to record structured metrics on Chrome.
//
// This class unifies all platform (currently lacros/ash chrome) specific
// recorder implementations under a common API.
//
// This class delegates to a Recorder that will be created on ctor.
// |Initialize()| should be called ASAP. When |Initialize()| should be called is
// platform specific.
class ChromeStructuredMetricsDelegate : public RecordingDelegate {
 public:
  // Pointer to singleton.
  static ChromeStructuredMetricsDelegate* Get();

  ChromeStructuredMetricsDelegate(
      const ChromeStructuredMetricsDelegate& recorder) = delete;
  ChromeStructuredMetricsDelegate& operator=(
      const ChromeStructuredMetricsDelegate& recorder) = delete;

  // Initializes the recorder. If this is called more than once, this operation
  // will no-op. This must be called before any recording is done or the record
  // operations will no-op.
  //
  // For Ash Chrome, this should be called after Crosapi is initialized.
  //
  // For Lacros Chrome, this should be called after LacrosService has been
  // created. This should also be called on the main thread (UI).
  void Initialize();

  // RecordingDelegate:
  void RecordEvent(Event&& event) override;
  bool IsReadyToRecord() const override;

 private:
  ChromeStructuredMetricsDelegate();
  ~ChromeStructuredMetricsDelegate() override;

  friend class base::NoDestructor<ChromeStructuredMetricsDelegate>;
  friend class LacrosStructuredMetricsDelegateTest;

  std::unique_ptr<RecordingDelegate> delegate_;
  bool is_initialized_ = false;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_CHROME_STRUCTURED_METRICS_DELEGATE_H_
