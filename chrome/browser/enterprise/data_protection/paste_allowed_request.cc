// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/paste_allowed_request.h"

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace enterprise_data_protection {

namespace {

PasteAllowedRequest::RequestsMap& RequestsMapStorage() {
  static base::NoDestructor<PasteAllowedRequest::RequestsMap> requests;
  return *requests.get();
}

// Completion callback of `StartPasteAllowedRequest()`. Sets the allowed
// status for the clipboard data corresponding to sequence number `seqno`.
void FinishPasteIfAllowed(
    const content::GlobalRenderFrameHostId& rfh_id,
    const ui::ClipboardSequenceNumberToken& seqno,
    std::optional<content::ClipboardPasteData> clipboard_paste_data) {
  if (!RequestsMapStorage().contains(rfh_id) ||
      !RequestsMapStorage().at(rfh_id).contains(seqno)) {
    return;
  }

  auto& request = RequestsMapStorage()[rfh_id][seqno];
  request.Complete(std::move(clipboard_paste_data));
}

}  // namespace

// The amount of time that the result of a content allow request is cached
// and reused for the same clipboard `seqno`.
// TODO(b/294844565): Update this once multi-format pastes are handled
// correctly.
constexpr base::TimeDelta PasteAllowedRequest::kIsPasteAllowedRequestTooOld =
    base::Seconds(5);

// static
void PasteAllowedRequest::StartPasteAllowedRequest(
    const content::ClipboardEndpoint& source,
    const content::ClipboardEndpoint& destination,
    const content::ClipboardMetadata& metadata,
    content::ClipboardPasteData clipboard_paste_data,
    IsClipboardPasteAllowedCallback callback) {
  DCHECK(destination.web_contents());
  DCHECK(destination.web_contents()->GetPrimaryMainFrame());

  CleanupObsoleteRequests();

  ui::ClipboardSequenceNumberToken seqno = metadata.seqno;
  content::GlobalRenderFrameHostId rfh_id =
      destination.web_contents()->GetPrimaryMainFrame()->GetGlobalId();

  // Add |callback| to the callbacks associated to the sequence number, adding
  // an entry to the map if one does not exist.
  PasteAllowedRequest& request = RequestsMapStorage()[rfh_id][seqno];

  // If this request has already completed, invoke the callback immediately
  // and return.
  if (request.is_complete()) {
    request.InvokeCallback(std::move(clipboard_paste_data),
                           std::move(callback));
    return;
  }

  if (request.AddCallback(std::move(callback))) {
    PasteIfAllowedByPolicy(
        source, destination, metadata, std::move(clipboard_paste_data),
        base::BindOnce(&FinishPasteIfAllowed, rfh_id, seqno));
  } else {
    request.AddData(std::move(clipboard_paste_data));
  }
}

// static
void PasteAllowedRequest::CleanupObsoleteRequests() {
  for (auto rfh_it = RequestsMapStorage().begin();
       rfh_it != RequestsMapStorage().end();) {
    for (auto seqno_request_it = rfh_it->second.begin();
         seqno_request_it != rfh_it->second.end();) {
      seqno_request_it = seqno_request_it->second.IsObsolete(base::Time::Now())
                             ? rfh_it->second.erase(seqno_request_it)
                             : std::next(seqno_request_it);
    }
    rfh_it = rfh_it->second.empty() ? RequestsMapStorage().erase(rfh_it)
                                    : std::next(rfh_it);
  }
}

// static
void PasteAllowedRequest::CleanupRequestsForTesting() {
  RequestsMapStorage().clear();
}

// static
void PasteAllowedRequest::AddRequestToCacheForTesting(
    content::GlobalRenderFrameHostId rfh_id,
    ui::ClipboardSequenceNumberToken seqno,
    PasteAllowedRequest request) {
  RequestsMapStorage()[rfh_id].emplace(seqno, std::move(request));
}

// static
size_t PasteAllowedRequest::requests_count_for_testing() {
  size_t total = 0;
  for (auto& entry : RequestsMapStorage()) {
    total += entry.second.size();
  }
  return total;
}

PasteAllowedRequest::PasteAllowedRequest() = default;
PasteAllowedRequest::PasteAllowedRequest(PasteAllowedRequest&&) = default;
PasteAllowedRequest& PasteAllowedRequest::operator=(PasteAllowedRequest&&) =
    default;
PasteAllowedRequest::~PasteAllowedRequest() = default;

bool PasteAllowedRequest::AddCallback(
    IsClipboardPasteAllowedCallback callback) {
  callbacks_.push_back(std::move(callback));

  // If this is the first callback registered tell the caller to start the scan.
  return callbacks_.size() == 1;
}

void PasteAllowedRequest::AddData(content::ClipboardPasteData data) {
  data_.Merge(std::move(data));
}

void PasteAllowedRequest::InvokeCallback(
    content::ClipboardPasteData data,
    IsClipboardPasteAllowedCallback callback) {
  DCHECK(is_complete());

  if (*data_allowed_) {
    // It's possible the completed request had its `data_` replaced, so merging
    // will override `data` with any non-empty field in `data_` as needed.
    data.Merge(data_);
    std::move(callback).Run(std::move(data));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void PasteAllowedRequest::Complete(
    std::optional<content::ClipboardPasteData> data) {
  completed_time_ = base::Time::Now();
  data_allowed_ = data.has_value();
  if (*data_allowed_) {
    AddData(std::move(*data));
  }
  InvokeCallbacks();
}

bool PasteAllowedRequest::IsObsolete(base::Time now) {
  if (!is_complete()) {
    return false;
  }
  return (now - completed_time_) > kIsPasteAllowedRequestTooOld;
}

base::Time PasteAllowedRequest::completed_time() const {
  DCHECK(is_complete());
  return completed_time_;
}

void PasteAllowedRequest::InvokeCallbacks() {
  DCHECK(data_allowed_.has_value());

  auto callbacks = std::move(callbacks_);
  for (auto& callback : callbacks) {
    if (!callback.is_null()) {
      if (*data_allowed_) {
        std::move(callback).Run(data_);
      } else {
        std::move(callback).Run(std::nullopt);
      }
    }
  }
}

}  // namespace enterprise_data_protection
