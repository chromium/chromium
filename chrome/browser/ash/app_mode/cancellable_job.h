// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_CANCELLABLE_JOB_H_
#define CHROME_BROWSER_ASH_APP_MODE_CANCELLABLE_JOB_H_

namespace ash {

// Interface to represent long running asynchronous operations that are
// cancellable.
//
// The only public member of a CancellableJob is its destructor. Destruction
// implies the job should be cancelled if it's still in progress.
//
// This is useful so that implementers can hide internal details while allowing
// the asynchronous job to be deleted and cancelled by its owners.
class [[nodiscard]] CancellableJob {
 public:
  virtual ~CancellableJob() = default;
  CancellableJob(const CancellableJob&) = delete;
  CancellableJob& operator=(const CancellableJob&) = delete;

 protected:
  CancellableJob() = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_CANCELLABLE_JOB_H_
