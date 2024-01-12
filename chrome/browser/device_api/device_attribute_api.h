// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_DEVICE_ATTRIBUTE_API_H_
#define CHROME_BROWSER_DEVICE_API_DEVICE_ATTRIBUTE_API_H_

#include "base/functional/callback_forward.h"
#include "third_party/blink/public/mojom/device/device.mojom.h"

namespace device_attribute_api {

void ReportNotAffiliatedError(
    base::OnceCallback<void(blink::mojom::DeviceAttributeResultPtr)> callback);
void ReportNotAllowedError(
    base::OnceCallback<void(blink::mojom::DeviceAttributeResultPtr)> callback);
void GetDirectoryId(
    blink::mojom::DeviceAPIService::GetDirectoryIdCallback callback);
void GetHostname(blink::mojom::DeviceAPIService::GetHostnameCallback callback);
void GetSerialNumber(
    blink::mojom::DeviceAPIService::GetSerialNumberCallback callback);
void GetAnnotatedAssetId(
    blink::mojom::DeviceAPIService::GetAnnotatedAssetIdCallback callback);
void GetAnnotatedLocation(
    blink::mojom::DeviceAPIService::GetAnnotatedLocationCallback callback);

}  // namespace device_attribute_api

#endif  // CHROME_BROWSER_DEVICE_API_DEVICE_ATTRIBUTE_API_H_
