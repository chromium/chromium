// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/install_event_log_uploader_base.h"

#include <algorithm>

namespace policy {

namespace {

// The backoff time starts at |kMinRetryBackoffMs| and doubles after each upload
// failure until it reaches |kMaxRetryBackoffMs|, from which point on it remains
// constant. The backoff is reset to |kMinRetryBackoffMs| after the next
// successful upload or if the upload request is canceled.
const int kMinRetryBackoffMs = 10 * 1000;            // 10 seconds
const int kMaxRetryBackoffMs = 24 * 60 * 60 * 1000;  // 24 hours

}  // namespace

InstallEventLogUploaderBase::InstallEventLogUploaderBase(
    CloudPolicyClient* client,
    Profile* profile)
    : client_(client),
      profile_(profile),
      retry_backoff_ms_(kMinRetryBackoffMs) {
  DCHECK(client_);
  client_->AddObserver(this);
}

InstallEventLogUploaderBase::~InstallEventLogUploaderBase() {
  client_->RemoveObserver(this);
}

void InstallEventLogUploaderBase::RequestUpload() {
  CheckDelegateSet();
  if (upload_requested_)
    return;

  upload_requested_ = true;
  if (client_->is_registered())
    StartSerialization();
}

void InstallEventLogUploaderBase::CancelUpload() {
  CancelClientUpload();
  upload_requested_ = false;
  retry_backoff_ms_ = kMinRetryBackoffMs;
}

void InstallEventLogUploaderBase::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  if (!upload_requested_)
    return;

  if (client->is_registered()) {
    StartSerialization();
  } else {
    CancelUpload();
    RequestUpload();
  }
}

void InstallEventLogUploaderBase::OnUploadDone(bool success) {
  if (success) {
    upload_requested_ = false;
    retry_backoff_ms_ = kMinRetryBackoffMs;
    OnUploadSuccess();
    return;
  }
  PostTaskForStartSerialization();
  retry_backoff_ms_ = std::min(retry_backoff_ms_ << 1, kMaxRetryBackoffMs);
}

}  // namespace policy
