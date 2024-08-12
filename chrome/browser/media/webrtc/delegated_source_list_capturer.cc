// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/delegated_source_list_capturer.h"

DelegatedSourceListCapturer::DelegatedSourceListCapturer() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DelegatedSourceListCapturer::~DelegatedSourceListCapturer() = default;

void DelegatedSourceListCapturer::Start(Callback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DelegatedSourceListCapturer::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool DelegatedSourceListCapturer::GetSourceList(SourceList* sources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40286360): Implement
  return true;
}

webrtc::DelegatedSourceListController*
DelegatedSourceListCapturer::GetDelegatedSourceListController() {
  // Returning this is safe also on other sequences.
  DETACH_FROM_SEQUENCE(sequence_checker_);
  return this;
}

void DelegatedSourceListCapturer::Observe(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!delegated_source_list_observer_ || !observer);
  delegated_source_list_observer_ = observer;
}

void DelegatedSourceListCapturer::EnsureVisible() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40286360): Implement
}

void DelegatedSourceListCapturer::EnsureHidden() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40286360): Implement or ensure this method is not called.
}
