// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/delegated_source_list_capturer.h"

#include "base/task/bind_post_task.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"

// Posts a task to stop observing the `source_id` in the Native OS picker. This
// might close the picker if it is not observing any other sources.
static void StopObservingSource(content::DesktopMediaID source_id) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&content::desktop_capture::CloseNativeScreenCapturePicker,
                     source_id));
}

DelegatedSourceListCapturer::DelegatedSourceListCapturer(
    content::DesktopMediaID::Type type)
    : type_(type) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DelegatedSourceListCapturer::~DelegatedSourceListCapturer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The picker should not observe the `source_id_` if this instance is
  // destructed without ever selecting a capture surface.
  if (!selected_source_ && source_id_) {
    StopObservingSource(content::DesktopMediaID(type_, *source_id_));
  }
}

void DelegatedSourceListCapturer::Start(Callback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DelegatedSourceListCapturer::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool DelegatedSourceListCapturer::GetSourceList(SourceList* sources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sources->clear();
  if (selected_source_) {
    sources->push_back(*selected_source_);
  }
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
  auto created_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&DelegatedSourceListCapturer::OnPickerCreated,
                     weak_ptr_factory_.GetWeakPtr()));
  auto picker_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&DelegatedSourceListCapturer::OnSelected,
                     weak_ptr_factory_.GetWeakPtr()));
  auto cancel_callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&DelegatedSourceListCapturer::OnCancelled,
                     weak_ptr_factory_.GetWeakPtr()));
  auto error_callback =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&DelegatedSourceListCapturer::OnError,
                                        weak_ptr_factory_.GetWeakPtr()));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&content::desktop_capture::OpenNativeScreenCapturePicker,
                     type_, std::move(created_callback),
                     std::move(picker_callback), std::move(cancel_callback),
                     std::move(error_callback)));
}

void DelegatedSourceListCapturer::EnsureHidden() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40286360): Implement or ensure this method is not called.
}

void DelegatedSourceListCapturer::OnPickerCreated(
    content::DesktopMediaID::Id source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only one source should be associated with the |DelegatedSourceListCapturer|
  // at a time.
  if (source_id_) {
    StopObservingSource(content::DesktopMediaID(type_, *source_id_));
  }
  source_id_ = source_id;
}

void DelegatedSourceListCapturer::OnSelected(Source source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  selected_source_ = source;
  if (delegated_source_list_observer_) {
    delegated_source_list_observer_->OnSelection();
  }
}

void DelegatedSourceListCapturer::OnCancelled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegated_source_list_observer_) {
    delegated_source_list_observer_->OnCancelled();
  }
}

void DelegatedSourceListCapturer::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegated_source_list_observer_) {
    delegated_source_list_observer_->OnError();
  }
}
