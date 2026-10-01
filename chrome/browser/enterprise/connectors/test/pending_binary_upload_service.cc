// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/pending_binary_upload_service.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_ack.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_cancel_requests.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/keyed_service/core/keyed_service.h"

namespace enterprise_connectors::test {

PendingBinaryUploadService::PendingBinaryUploadService() = default;

PendingBinaryUploadService::~PendingBinaryUploadService() = default;

// static
std::unique_ptr<KeyedService> PendingBinaryUploadService::Create(
    content::BrowserContext* context) {
  return std::make_unique<PendingBinaryUploadService>();
}

// static
PendingBinaryUploadService* PendingBinaryUploadService::GetForProfile(
    Profile* profile) {
  return static_cast<PendingBinaryUploadService*>(
      safe_browsing::CloudBinaryUploadServiceFactory::GetForProfile(profile));
}

void PendingBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<BinaryUploadRequest> request) {
  requests_.push_back(std::move(request));
}

void PendingBinaryUploadService::MaybeAcknowledge(
    std::unique_ptr<BinaryUploadAck> ack) {}

void PendingBinaryUploadService::MaybeCancelRequests(
    std::unique_ptr<BinaryUploadCancelRequests> cancel) {}

base::WeakPtr<BinaryUploadService> PendingBinaryUploadService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace enterprise_connectors::test
