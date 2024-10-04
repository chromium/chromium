// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_CONTROLLER_H_
#define ASH_SCANNER_SCANNER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/scanner/scanner_session.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

class ScannerDelegate;

// This is the top level controller used for Scanner. It acts as a mediator
// between Scanner and any consuming features.
class ASH_EXPORT ScannerController {
 public:
  explicit ScannerController(std::unique_ptr<ScannerDelegate> delegate);
  ScannerController(const ScannerController&) = delete;
  ScannerController& operator=(const ScannerController&) = delete;
  ~ScannerController();

  static bool IsEnabled();

  // Creates a new ScannerSession and returns a pointer to the created session.
  // Note that the created session is owned by the ScannerController. If the
  // Scanner cannot be initialized due to system level constraints (e.g. pref
  // disabled, feature not allowed), then no session is created and `nullptr` is
  // returned instead.
  ScannerSession* StartNewSession();

  // Fetches Scanner actions that are available based on the current
  // `scanner_session_` and the contents of `jpeg_bytes`. The actions are
  // returned via `callback`. If no session is active, then `callback` will be
  // run with an empty list of actions.
  void FetchActionsForImage(scoped_refptr<base::RefCountedMemory> jpeg_bytes,
                            ScannerSession::FetchActionsCallback callback);

  // Should be called when the user has finished interacting with a Scanner
  // session. This will trigger relevant cleanup and eventually destroy the
  // scanner session.
  void OnSessionUIClosed();

  bool HasActiveSessionForTesting() const;

  ScannerDelegate* delegate_for_testing() { return delegate_.get(); }

 private:
  std::unique_ptr<ScannerDelegate> delegate_;

  // May hold an active Scanner session, to allow access to the Scanner feature.
  std::unique_ptr<ScannerSession> scanner_session_;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_CONTROLLER_H_
