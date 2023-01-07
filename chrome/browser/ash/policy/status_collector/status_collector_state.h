// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_STATE_H_
#define CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_STATE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/policy/status_collector/status_collector.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// Helper class for state tracking of status queries (sync and async). Creates
// status blobs in the constructor and sends them to the the status response
// callback in the destructor.
//
// Some methods queue async queries to collect data. The response callback of
// these queries holds a reference to the instance of this class, so that the
// destructor will not be invoked and the status response callback will not be
// fired until the original owner of the instance releases its reference and all
// async queries finish.
//
// Therefore, if you create an instance of this class, make sure to release your
// reference after querying all async queries (if any), e.g. by using a local
// |scoped_refptr<StatusCollectorState>| and letting it go out of scope.
class StatusCollectorState
    : public base::RefCountedThreadSafe<StatusCollectorState> {
 public:
  explicit StatusCollectorState(
      const scoped_refptr<base::SequencedTaskRunner> task_runner,
      StatusCollectorCallback response);

  // Returns a reference to the internal state of this object.
  StatusCollectorParams& response_params();

 protected:
  friend class base::RefCountedThreadSafe<StatusCollectorState>;

  // Posts the response on the UI thread. As long as there is an outstanding
  // async query, the query holds a reference to us, so the destructor is
  // not called.
  virtual ~StatusCollectorState();

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  StatusCollectorCallback response_;
  StatusCollectorParams response_params_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_STATUS_COLLECTOR_STATE_H_
