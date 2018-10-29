// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_test_util.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/url_request/url_request_context.h"

namespace nqe_test_util {

namespace {

// Reports |type| to all of NetworkQualityEstimator's
// EffectiveConnectionTypeObservers.
void OverrideEffectiveConnectionTypeOnIO(net::EffectiveConnectionType type,
                                         IOThread* io_thread) {
  net::NetworkQualityEstimator* network_quality_estimator =
      io_thread->globals()->system_request_context->network_quality_estimator();
  if (!network_quality_estimator)
    return;
  network_quality_estimator->ReportEffectiveConnectionTypeForTesting(type);
}

void OverrideRTTsAndWaitOnIO(base::TimeDelta rtt, IOThread* io_thread) {
  net::NetworkQualityEstimator* network_quality_estimator =
      io_thread->globals()->system_request_context->network_quality_estimator();
  if (!network_quality_estimator)
    return;
  network_quality_estimator->ReportRTTsAndThroughputForTesting(rtt, rtt, -1);
}

}  // namespace

void OverrideEffectiveConnectionTypeAndWait(net::EffectiveConnectionType type) {
  // Block |run_loop| until OverrideEffectiveConnectionTypeOnIO has completed.
  // Any UI tasks posted by calling OverrideEffectiveConnectionTypeOnIO will
  // complete before the reply unblocks |run_loop|.
  base::RunLoop run_loop;
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&OverrideEffectiveConnectionTypeOnIO, type,
                     g_browser_process->io_thread()),
      run_loop.QuitClosure());
  run_loop.Run();
}

void OverrideRTTsAndWait(base::TimeDelta rtt) {
  // Block |run_loop| until OverrideRTTsAndWaitOnIO has completed.
  // Any UI tasks posted by calling OverrideRTTsAndWaitOnIO will complete before
  // the reply unblocks |run_loop|.
  base::RunLoop run_loop;
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&OverrideRTTsAndWaitOnIO, rtt,
                     g_browser_process->io_thread()),
      run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace nqe_test_util
