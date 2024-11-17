// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_FAKE_SCANNER_PROFILE_SCOPED_DELEGATE_H_
#define ASH_SCANNER_FAKE_SCANNER_PROFILE_SCOPED_DELEGATE_H_

#include <utility>

#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/manta/scanner_provider.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

class GaiaUrlsOverriderForTesting;

namespace google_apis {
class RequestSender;
}

namespace net::test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace net::test_server

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

  // Sets the callback called when the HTTP server for the Google APIs request
  // sender is called.
  void SetRequestCallback(
      net::test_server::EmbeddedTestServer::HandleRequestCallback
          request_callback) {
    request_callback_ = std::move(request_callback);
  }

  // ScannerProfileScopedDelegate:
  MOCK_METHOD(ScannerSystemState, GetSystemState, (), (const, override));
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
  MOCK_METHOD(void,
              FetchActionDetailsForImage,
              (scoped_refptr<base::RefCountedMemory> jpeg_bytes,
               manta::proto::ScannerAction selected_action,
               manta::ScannerProvider::ScannerProtoResponseCallback callback),
              (override));
  drive::DriveServiceInterface* GetDriveService() override;
  google_apis::RequestSender* GetGoogleApisRequestSender() override;
  bool IsGoogler() override;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  drive::FakeDriveService drive_service_;

  // Initialising these members will *start an HTTP server*, which is not needed
  // for most tests. These members are only initialised upon the first
  // `GetGoogleApisRequestSender` call. These members should either be all null,
  // or all non-null.
  std::unique_ptr<google_apis::RequestSender> request_sender_;
  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  std::unique_ptr<GaiaUrlsOverriderForTesting> gaia_urls_overrider_;

  net::test_server::EmbeddedTestServer::HandleRequestCallback request_callback_;
};

}  // namespace ash

#endif  // ASH_SCANNER_FAKE_SCANNER_PROFILE_SCOPED_DELEGATE_H_
