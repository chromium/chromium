// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/install_event_log_uploader_base.h"

#include <algorithm>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"

namespace policy {

namespace {

// The backoff time starts at |kMinRetryBackoffMs| and doubles after each upload
// failure until it reaches |kMaxRetryBackoffMs|, from which point on it remains
// constant. The backoff is reset to |kMinRetryBackoffMs| after the next
// successful upload or if the upload request is canceled.
const int kMinRetryBackoffMs = 10 * 1000;            // 10 seconds
const int kMaxRetryBackoffMs = 24 * 60 * 60 * 1000;  // 24 hours

// If install-log-fast-upload-for-tests flag is enabled, do not increase retry
// backoff.
bool FastUploadForTestsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kInstallLogFastUploadForTests);
}

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

InstallEventLogUploaderBase::InstallEventLogUploaderBase(Profile* profile)
    : client_(nullptr),
      profile_(profile),
      retry_backoff_ms_(kMinRetryBackoffMs) {}

InstallEventLogUploaderBase::~InstallEventLogUploaderBase() {
  if (client_)
    client_->RemoveObserver(this);
}

void InstallEventLogUploaderBase::RequestUpload() {
  CheckDelegateSet();
  if (upload_requested_)
    return;

  upload_requested_ = true;

  // If the client is set - ensure that it is also registered.
  // Otherwise start Serialization.
  if ((client_ && client_->is_registered()) || !client_) {
    StartSerialization();
  }
}

void InstallEventLogUploaderBase::CancelUpload() {
  CancelClientUpload();
  upload_requested_ = false;
  retry_backoff_ms_ = kMinRetryBackoffMs;
}

void InstallEventLogUploaderBase::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK(client_);
  if (!upload_requested_)
    return;

  if (client->is_registered()) {
    StartSerialization();
  } else {
    CancelUpload();
    RequestUpload();
  }
}

void InstallEventLogUploaderBase::OnUploadDone(
    CloudPolicyClient::Result result) {
  // TODO(b/256553070): Do not crash if the client is unregistered.
  CHECK(!result.IsClientNotRegisteredError());

  if (result.IsSuccess()) {
    upload_requested_ = false;
    retry_backoff_ms_ = kMinRetryBackoffMs;
    OnUploadSuccess();
    return;
  }

  PostTaskForStartSerialization();
  if (FastUploadForTestsEnabled())
    return;
  retry_backoff_ms_ = std::min(retry_backoff_ms_ << 1, kMaxRetryBackoffMs);
}

}  // namespace policy
