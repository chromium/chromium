// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_INSERTION_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_INSERTION_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace {

using StatusCallback = base::OnceCallback<void(bool)>;

}  // namespace

class LobsterInsertion {
 public:
  explicit LobsterInsertion(const std::string& image_bytes,
                            StatusCallback insert_status_callback);
  ~LobsterInsertion();

  // To help ensure that we do not insert the pending image in text fields other
  // then the target text input, we set a timeout for the insertion
  // operation to occur within otherwise it is cancelled. This method returns
  // the state of that timeout.
  bool HasTimedOut();

  // Attempts to commit the pending image into the currently focused text input.
  bool Commit();

 private:
  enum State {
    kPending,
    kTimedOut,
  };

  // This method is invoked after the insertion timeout has elapsed, and
  // it renders this operation inert. Consumers of this class can no longer
  // commit the pending image once this method has been called.
  void CancelInsertion();

  const std::string pending_image_bytes_;
  State state_;
  base::OneShotTimer insertion_timeout_;
  StatusCallback insert_status_callback_;
  base::WeakPtrFactory<LobsterInsertion> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_INSERTION_H_
