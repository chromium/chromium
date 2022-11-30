// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_DEVICE_ATTRIBUTE_API_H_
#define CHROME_BROWSER_DEVICE_API_DEVICE_ATTRIBUTE_API_H_

#include "third_party/blink/public/mojom/device/device.mojom.h"

using blink::mojom::DeviceAPIService;
using blink::mojom::DeviceAttributeResultPtr;

namespace device_attribute_api {

void ReportNotAffiliatedError(
    base::OnceCallback<void(DeviceAttributeResultPtr)> callback);
void ReportNotAllowedError(
    base::OnceCallback<void(DeviceAttributeResultPtr)> callback);
void GetDirectoryId(DeviceAPIService::GetDirectoryIdCallback callback);
void GetHostname(DeviceAPIService::GetHostnameCallback callback);
void GetSerialNumber(DeviceAPIService::GetSerialNumberCallback callback);
void GetAnnotatedAssetId(
    DeviceAPIService::GetAnnotatedAssetIdCallback callback);
void GetAnnotatedLocation(
    DeviceAPIService::GetAnnotatedLocationCallback callback);

}  // namespace device_attribute_api

#endif  // CHROME_BROWSER_DEVICE_API_DEVICE_ATTRIBUTE_API_H_
