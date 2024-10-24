// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_FAKE_SCANNER_PROFILE_SCOPED_DELEGATE_H_
#define ASH_SCANNER_FAKE_SCANNER_PROFILE_SCOPED_DELEGATE_H_

#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/manta/scanner_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// A fake ScannerProfileScopedDelegate which can be used in tests.
class FakeScannerProfileScopedDelegate : public ScannerProfileScopedDelegate {
 public:
  FakeScannerProfileScopedDelegate();
  FakeScannerProfileScopedDelegate(const FakeScannerProfileScopedDelegate&) =
      delete;
  FakeScannerProfileScopedDelegate& operator=(
      const FakeScannerProfileScopedDelegate&) = delete;
  ~FakeScannerProfileScopedDelegate() override;

  // ScannerProfileScopedDelegate:
  ScannerSystemState GetSystemState() const override;
  // Use the following as a gMock action to run `callback` synchronously when
  // this method is called:
  //     base::test::RunOnceCallback<1>(scanner_output, manta_status)
  //
  // Use the following as a gMock action to get the `jpeg_bytes` and `callback`
  // values asynchronously.
  //     base::test::TestFuture<
  //         scoped_refptr<base::RefCountedMemory>,
  //         manta::ScannerProvider::ScannerProtoResponseCallback> future;
  //     // ...
  //     base::test::InvokeFuture(future)
  MOCK_METHOD(void,
              FetchActionsForImage,
              (scoped_refptr<base::RefCountedMemory> jpeg_bytes,
               manta::ScannerProvider::ScannerProtoResponseCallback callback),
              (override));
  drive::DriveServiceInterface* GetDriveService() override;

 private:
  drive::FakeDriveService drive_service_;
};

}  // namespace ash

#endif  // ASH_SCANNER_FAKE_SCANNER_PROFILE_SCOPED_DELEGATE_H_
