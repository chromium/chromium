// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_CHROME_STRUCTURED_METRICS_RECORDER_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_CHROME_STRUCTURED_METRICS_RECORDER_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/metrics/structured/structured_mojo_events.h"

namespace metrics {
namespace structured {

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
class ChromeStructuredMetricsRecorder : RecordingDelegate {
 public:
  // Pointer to singleton.
  static ChromeStructuredMetricsRecorder* Get();

  ChromeStructuredMetricsRecorder(
      const ChromeStructuredMetricsRecorder& recorder) = delete;
  ChromeStructuredMetricsRecorder& operator=(
      const ChromeStructuredMetricsRecorder& recorder) = delete;

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
  void Record(EventBase&& event_base) override;
  bool IsReadyToRecord() const override;

 private:
  ChromeStructuredMetricsRecorder();
  ~ChromeStructuredMetricsRecorder() override;

  friend class base::NoDestructor<ChromeStructuredMetricsRecorder>;
  friend class LacrosStructuredMetricsRecorderTest;

  std::unique_ptr<RecordingDelegate> delegate_;
  bool is_initialized_ = false;
};

}  // namespace structured
}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_CHROME_STRUCTURED_METRICS_RECORDER_H_
