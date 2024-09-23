// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_UPLOAD_DECLARATIONS_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_UPLOAD_DECLARATIONS_H_

#include <cstdint>
#include <list>

#include "base/functional/callback_forward.h"
#include "components/reporting/proto/synced/configuration_file.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// UploadEnqueuedCallback is used to pass information about enqueued upload
// and cache state.
using UploadEnqueuedCallback =
    base::OnceCallback<void(StatusOr<std::list<int64_t>>)>;

// ReportSuccessfulUploadCallback is used to pass server responses back to
// the caller (the response consists of sequence information and force_confirm
// flag).
using ReportSuccessfulUploadCallback =
    base::RepeatingCallback<void(SequenceInformation,
                                 /*force_confirm*/ bool)>;

// ReceivedEncryptionKeyCallback is called if server attached encryption key
// to the response.
using EncryptionKeyAttachedCallback =
    base::RepeatingCallback<void(SignedEncryptionInfo)>;

// ConfigFileAttachedCallback is called if the server attached a configuration
// file to the response. It passes the parsed response to the
// `ConfigurationFileController`.
using ConfigFileAttachedCallback = base::RepeatingCallback<void(ConfigFile)>;

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_UPLOAD_DECLARATIONS_H_
