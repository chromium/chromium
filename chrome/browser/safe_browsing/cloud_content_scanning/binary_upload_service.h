// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_

#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"

namespace safe_browsing {

// Forwarding alias to allow existing code to compile without changes.
using BinaryUploadService = ::enterprise_connectors::BinaryUploadService;
using BinaryUploadAck = ::enterprise_connectors::BinaryUploadAck;
using BinaryUploadCancelRequests =
    ::enterprise_connectors::BinaryUploadCancelRequests;
using BinaryUploadRequest = ::enterprise_connectors::BinaryUploadRequest;

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
