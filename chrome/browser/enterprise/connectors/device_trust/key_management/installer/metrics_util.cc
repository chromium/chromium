// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace enterprise_connectors {

namespace {

constexpr char kWithNonceVariant[] = "WithNonce";
constexpr char kNoNonceVariant[] = "NoNonce";

const char* GetVariant(bool is_rotation) {
  return is_rotation ? kWithNonceVariant : kNoNonceVariant;
}

}  // namespace

void RecordRotationStatus(bool is_rotation, RotationStatus status) {
  static constexpr char kStatusHistogramFormat[] =
      "Enterprise.DeviceTrust.RotateSigningKey.%s.Status";
  base::UmaHistogramEnumeration(
      base::StringPrintf(kStatusHistogramFormat, GetVariant(is_rotation)),
      status);
}

void RecordUploadCode(bool is_rotation, int status_code) {
  static constexpr char kUploadCodeHistogramFormat[] =
      "Enterprise.DeviceTrust.RotateSigningKey.%s.UploadCode";
  base::UmaHistogramSparse(
      base::StringPrintf(kUploadCodeHistogramFormat, GetVariant(is_rotation)),
      status_code);
}

}  // namespace enterprise_connectors
