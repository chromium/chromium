// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

// On Fuchsia, we assume that the Component Framework ensures that only a single
// Chrome component instance will run against a particular data-directory.
// This file contains a stubbed-out ProcessSingleton implementation. :)
//
// In future we will need to support a mechanism for URL launch attempts to
// be handled by a running Chrome instance, e.g. by registering the instance as
// the Runner for HTTP[S] component URLs.
//
// TODO(crbug.com/1370080): Implement these methods as appropriate.

ProcessSingleton::ProcessSingleton(
    const base::FilePath& user_data_dir,
    const NotificationCallback& notification_callback) {}

ProcessSingleton::~ProcessSingleton() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ProcessSingleton::NotifyResult ProcessSingleton::NotifyOtherProcess() {
  NOTIMPLEMENTED_LOG_ONCE();
  return PROCESS_NONE;
}

ProcessSingleton::NotifyResult ProcessSingleton::NotifyOtherProcessOrCreate() {
  NOTIMPLEMENTED_LOG_ONCE();
  return PROCESS_NONE;
}

bool ProcessSingleton::Create() {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

void ProcessSingleton::StartWatching() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ProcessSingleton::Cleanup() {
  NOTIMPLEMENTED_LOG_ONCE();
}
