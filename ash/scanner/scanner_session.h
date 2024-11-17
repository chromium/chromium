// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_SESSION_H_
#define ASH_SCANNER_SCANNER_SESSION_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "ash/scanner/scanner_unpopulated_action.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

class ScannerProfileScopedDelegate;

// A ScannerSession represents a single "use" of the Scanner feature. A session
// will be created when the feature is first triggered, until the feature is
// either dismissed, or commits its final result. The initialization of a
// session will be triggered on the creation of a new SunfishSession, however
// a ScannerSession's lifetime is not strictly bound to the lifetime of a
// SunfishSession.
class ASH_EXPORT ScannerSession : public ScannerCommandDelegate {
 public:
  // Callback used to receive the actions returned from a FetchActions call.
  using FetchActionsCallback =
      base::OnceCallback<void(std::vector<ScannerActionViewModel> actions)>;

  ScannerSession(ScannerProfileScopedDelegate* delegate);
  ScannerSession(const ScannerSession&) = delete;
  ScannerSession& operator=(const ScannerSession&) = delete;
  ~ScannerSession() override;

  // Fetches Scanner actions that are available based on the contents of
  // `jpeg_bytes`. The actions are returned via `callback`.
  void FetchActionsForImage(scoped_refptr<base::RefCountedMemory> jpeg_bytes,
                            FetchActionsCallback callback);

  // ScannerCommandDelegate:
  void OpenUrl(const GURL& url) override;
  drive::DriveServiceInterface* GetDriveService() override;
  void SetClipboard(std::unique_ptr<ui::ClipboardData> data) override;
  google_apis::RequestSender* GetGoogleApisRequestSender() override;

 private:
  void OnActionsReturned(
      scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes,
      base::TimeTicks request_start_time,
      FetchActionsCallback callback,
      std::unique_ptr<manta::proto::ScannerOutput> output,
      manta::MantaStatus status);

  // Populates the selected action. Used as a
  // `ScannerUnpopulatedAction::PopulateToProtoCallback` once bound with the
  // possibly-downscaled JPEG bytes.
  void PopulateAction(
      scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes,
      manta::proto::ScannerAction unpopulated_action,
      ScannerUnpopulatedAction::PopulatedProtoCallback callback);

  const raw_ptr<ScannerProfileScopedDelegate> delegate_;

  base::WeakPtrFactory<ScannerSession> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_SESSION_H_
