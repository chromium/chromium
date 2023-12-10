// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_LACROS_STRUCTURED_METRICS_DELEGATE_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_LACROS_STRUCTURED_METRICS_DELEGATE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace metrics::structured {

// Structured metrics recording delegate for Lacros. This class forwards all
// events to Ash Chrome, where the main service to validate and persist events
// is.
//
// This recorder is thread-safe. Any calls to |this| not on the dedicated
// sequence will be forwarded to the dedicated sequence set by |SetSequence|.
class LacrosStructuredMetricsDelegate
    : public StructuredMetricsClient::RecordingDelegate {
 public:
  LacrosStructuredMetricsDelegate();
  ~LacrosStructuredMetricsDelegate() override;

  LacrosStructuredMetricsDelegate(
      const LacrosStructuredMetricsDelegate& recorder) = delete;
  LacrosStructuredMetricsDelegate& operator=(
      const LacrosStructuredMetricsDelegate& recorder) = delete;

  // Assigns a sequence for which events will be recorded. Should be called
  // before any calls to Record. This sequence should be the affine sequence.
  void SetSequence(
      const scoped_refptr<base::SequencedTaskRunner> sequence_task_runner);

  // RecordingDelegate:
  void RecordEvent(Event&& event) override;
  bool IsReadyToRecord() const override;

 private:
  friend class LacrosStructuredMetricsDelegateTest;

  // For testing.
  class Observer : public base::CheckedObserver {
   public:
    // Called on every call of |RecordEvent|.
    virtual void OnRecord(const Event& event) = 0;

    // Called when |enqueued_events_| is flushed.
    virtual void OnFlush() = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  scoped_refptr<base::SequencedTaskRunner> sequence_task_runner_;

  // Lacros service may not be ready by the time some metrics need to be
  // recorded. Enqueue those events in memory until the service is ready.
  //
  // TODO(jongahn): Think about having |this| own a remote and expose an API in
  // LacrosService to allow arbitrary receivers be bound. This could allow other
  // processes utilize Structured Metrics.
  std::vector<Event> enqueued_events_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<LacrosStructuredMetricsDelegate> weak_ptr_factory_{this};
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_LACROS_STRUCTURED_METRICS_DELEGATE_H_
