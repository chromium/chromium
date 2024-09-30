// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_SESSION_H_
#define ASH_SCANNER_SCANNER_SESSION_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"

namespace ash {

class ScannerProfileScopedDelegate;

// A ScannerSession represents a single "use" of the Scanner feature. A session
// will be created when the feature is first triggered, until the feature is
// either dismissed, or commits its final result. The initialization of a
// session will be triggered on the creation of a new SunfishSession, however
// a ScannerSession's lifetime is not strictly bound to the lifetime of a
// SunfishSession.
class ASH_EXPORT ScannerSession {
 public:
  // Callback used to receive the actions returned from a FetchActions call.
  using FetchActionsCallback =
      base::OnceCallback<void(std::vector<ScannerAction>)>;

  ScannerSession(ScannerProfileScopedDelegate* delegate);
  ScannerSession(const ScannerSession&) = delete;
  ScannerSession& operator=(const ScannerSession&) = delete;
  ~ScannerSession();

  // Fetches Scanner actions that are available based on the contents of
  // `jpeg_bytes`. The actions are returned via `callback`.
  void FetchActionsForImage(scoped_refptr<base::RefCountedMemory> jpeg_bytes,
                            FetchActionsCallback callback);

 private:
  void OnActionsReturned(
      FetchActionsCallback callback,
      base::expected<std::vector<ScannerAction>, ScannerError> returned);

  const raw_ptr<ScannerProfileScopedDelegate> delegate_;

  base::WeakPtrFactory<ScannerSession> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_SESSION_H_
