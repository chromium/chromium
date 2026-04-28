// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crosapi/document_scan_ash_type_converters.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

namespace {

// Supports the static_cast() in ProtobufResultToMojoResult() below.
static_assert(lorgnette::SCAN_FAILURE_MODE_NO_FAILURE ==
              static_cast<int>(mojom::ScanFailureMode::kNoFailure));
static_assert(lorgnette::SCAN_FAILURE_MODE_UNKNOWN ==
              static_cast<int>(mojom::ScanFailureMode::kUnknown));
static_assert(lorgnette::SCAN_FAILURE_MODE_DEVICE_BUSY ==
              static_cast<int>(mojom::ScanFailureMode::kDeviceBusy));
static_assert(lorgnette::SCAN_FAILURE_MODE_ADF_JAMMED ==
              static_cast<int>(mojom::ScanFailureMode::kAdfJammed));
static_assert(lorgnette::SCAN_FAILURE_MODE_ADF_EMPTY ==
              static_cast<int>(mojom::ScanFailureMode::kAdfEmpty));
static_assert(lorgnette::SCAN_FAILURE_MODE_FLATBED_OPEN ==
              static_cast<int>(mojom::ScanFailureMode::kFlatbedOpen));
static_assert(lorgnette::SCAN_FAILURE_MODE_IO_ERROR ==
              static_cast<int>(mojom::ScanFailureMode::kIoError));

}  // namespace

DocumentScanAsh::DocumentScanAsh() = default;

DocumentScanAsh::~DocumentScanAsh() = default;

void DocumentScanAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DocumentScan> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

}  // namespace crosapi
