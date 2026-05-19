// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_url_loader_throttle.h"

#include <unistd.h>

#include <algorithm>
#include <limits>

#include "android_webview/browser/content_restriction/aw_content_restriction_blocked_navigation_tracker.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/data_element.h"

namespace android_webview {
namespace {

void WriteDataElementBytesToPipe(
    int write_fd,
    const network::DataElementBytes* bytes,
    scoped_refptr<network::ResourceRequestBody> request_body) {
  base::ScopedFD fd(write_fd);
  DCHECK(bytes);
  if (!base::WriteFileDescriptor(fd.get(), bytes->AsStringView())) {
    LOG(ERROR) << "Failed to write data element bytes to pipe";
  }
}

void WriteDataElementFileToPipe(
    int write_fd,
    const network::DataElementFile* file_element,
    scoped_refptr<network::ResourceRequestBody> request_body) {
  base::ScopedFD fd(write_fd);
  DCHECK(file_element);
  base::File file(file_element->path(),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open file for streaming request body: "
               << file_element->path().value();
    return;
  }

  uint64_t offset = file_element->offset();
  uint64_t length = file_element->length();
  if (length == std::numeric_limits<uint64_t>::max()) {
    // Stream the entire remainder of the file.
    int64_t file_len = file.GetLength();
    if (file_len < 0 || offset > static_cast<uint64_t>(file_len)) {
      return;
    }
    length = static_cast<uint64_t>(file_len) - offset;
  }

  if (file.Seek(base::File::FROM_BEGIN, static_cast<int64_t>(offset)) < 0) {
    return;
  }

  // Stream the file contents in chunks to minimize memory footprint.
  uint8_t buffer[4096];
  uint64_t total_read = 0;
  while (total_read < length) {
    size_t to_read =
        std::min(sizeof(buffer), static_cast<size_t>(length - total_read));
    std::optional<size_t> read_result =
        file.ReadAtCurrentPos(base::span(buffer).first(to_read));
    if (!read_result.has_value() || read_result.value() == 0) {
      return;
    }
    if (!base::WriteFileDescriptor(
            fd.get(), std::string_view(reinterpret_cast<const char*>(buffer),
                                       read_result.value()))) {
      return;
    }
    total_read += read_result.value();
  }
}

void WriteRequestBodyToPipe(
    int write_fd,
    scoped_refptr<network::ResourceRequestBody> request_body) {
  if (!request_body) {
    close(write_fd);
    return;
  }

  DCHECK(request_body->elements());
  for (const network::DataElement& element : *request_body->elements()) {
    switch (element.type()) {
      case network::DataElement::Tag::kBytes: {
        // Pass the `request_body` to guarantee safe memory access
        // until the request body is fully written to the pipe.
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
            base::BindOnce(&WriteDataElementBytesToPipe, write_fd,
                           &element.As<network::DataElementBytes>(),
                           request_body));
        break;
      }
      case network::DataElement::Tag::kFile: {
        // Pass the `request_body` to guarantee safe memory access
        // until file contents are fully written to the pipe.
        base::ThreadPool::PostTask(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
            base::BindOnce(&WriteDataElementFileToPipe, write_fd,
                           &element.As<network::DataElementFile>(),
                           request_body));
        break;
      }
      default: {
        close(write_fd);
        NOTREACHED()
            << "Unsupported request body data type for content restriction: "
            << static_cast<int>(element.type());
      }
    }
  }
}

}  // namespace

AwContentRestrictionURLLoaderThrottle::AwContentRestrictionURLLoaderThrottle(
    AwContentRestrictionManagerClient* client,
    AwContentRestrictionBlockedNavigationTracker* tracker,
    std::optional<int64_t> navigation_id)
    : content_restriction_manager_client_(client),
      tracker_(tracker),
      navigation_id_(navigation_id) {}

AwContentRestrictionURLLoaderThrottle::
    ~AwContentRestrictionURLLoaderThrottle() = default;

void AwContentRestrictionURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK(content_restriction_manager_client_);
  if (navigation_id_.has_value() &&
      content_restriction_manager_client_->IsContentRestrictionEnabled()) {
    *defer = true;

    const int64_t navigation_id = navigation_id_.value();
    if (request->request_body) {
      int write_fd = content_restriction_manager_client_
                         ->CreateRequestBodyPipeAndGetWriteFd(navigation_id);
      if (write_fd >= 0) {
        WriteRequestBodyToPipe(write_fd, request->request_body);
      }
    }

    content_restriction_manager_client_->RequestContentClassification(
        navigation_id, *request,
        base::BindOnce(
            &AwContentRestrictionURLLoaderThrottle::OnClassificationResult,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void AwContentRestrictionURLLoaderThrottle::OnClassificationResult(
    bool is_allowed) {
  DCHECK(delegate_);
  DCHECK(tracker_);
  if (is_allowed) {
    delegate_->Resume();
    return;
  }

  if (navigation_id_.has_value()) {
    tracker_->RegisterNavigationAsBlocked(navigation_id_.value());
  }
  delegate_->CancelWithError(net::ERR_BLOCKED_BY_CLIENT);
}

}  // namespace android_webview
