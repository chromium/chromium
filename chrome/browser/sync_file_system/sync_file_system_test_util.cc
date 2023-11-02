// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "content/public/test/test_utils.h"
#include "google_apis/common/api_error_codes.h"

using content::BrowserThread;

namespace sync_file_system {

namespace drive_backend {
class MetadataDatabase;
}  // namespace drive_backend

template <typename R>
void AssignAndQuit(base::RunLoop* run_loop, R* result_out, R result) {
  DCHECK(result_out);
  DCHECK(run_loop);
  *result_out = result;
  run_loop->Quit();
}

template <typename R>
base::OnceCallback<void(R)> AssignAndQuitCallback(base::RunLoop* run_loop,
                                                  R* result) {
  return base::BindOnce(&AssignAndQuit<R>, run_loop, base::Unretained(result));
}

template <typename Arg, typename Param>
void ReceiveResult1(bool* done, Arg* arg_out, Param arg) {
  EXPECT_FALSE(*done);
  *done = true;
  *arg_out = std::forward<Param>(arg);
}

template <typename Arg>
base::OnceCallback<void(typename TypeTraits<Arg>::ParamType)>
CreateResultReceiver(Arg* arg_out) {
  using Param = typename TypeTraits<Arg>::ParamType;
  return base::BindOnce(&ReceiveResult1<Arg, Param>,
                        base::Owned(new bool(false)), arg_out);
}

// Instantiate versions we know callers will need.
template base::OnceCallback<void(SyncStatusCode)> AssignAndQuitCallback(
    base::RunLoop*,
    SyncStatusCode*);

#define INSTANTIATE_RECEIVER(type) \
  template base::OnceCallback<void(type)> CreateResultReceiver(type*)
INSTANTIATE_RECEIVER(SyncStatusCode);
INSTANTIATE_RECEIVER(google_apis::ApiErrorCode);
INSTANTIATE_RECEIVER(std::unique_ptr<RemoteFileSyncService::OriginStatusMap>);
#undef INSTANTIATE_RECEIVER

}  // namespace sync_file_system
