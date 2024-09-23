// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_DEVICE_ATTRIBUTE_API_H_
#define CHROME_BROWSER_DEVICE_API_DEVICE_ATTRIBUTE_API_H_

#include "base/functional/callback_forward.h"
#include "third_party/blink/public/mojom/device/device.mojom.h"

class DeviceAttributeApi {
 public:
  DeviceAttributeApi() = default;
  DeviceAttributeApi(const DeviceAttributeApi&) = delete;
  DeviceAttributeApi& operator=(const DeviceAttributeApi&) = delete;
  virtual ~DeviceAttributeApi() = default;

  virtual void ReportNotAffiliatedError(
      base::OnceCallback<void(blink::mojom::DeviceAttributeResultPtr)>
          callback) = 0;
  virtual void ReportNotAllowedError(
      base::OnceCallback<void(blink::mojom::DeviceAttributeResultPtr)>
          callback) = 0;
  virtual void GetDirectoryId(
      blink::mojom::DeviceAPIService::GetDirectoryIdCallback callback) = 0;
  virtual void GetHostname(
      blink::mojom::DeviceAPIService::GetHostnameCallback callback) = 0;
  virtual void GetSerialNumber(
      blink::mojom::DeviceAPIService::GetSerialNumberCallback callback) = 0;
  virtual void GetAnnotatedAssetId(
      blink::mojom::DeviceAPIService::GetAnnotatedAssetIdCallback callback) = 0;
  virtual void GetAnnotatedLocation(
      blink::mojom::DeviceAPIService::GetAnnotatedLocationCallback
          callback) = 0;
};

class DeviceAttributeApiImpl : public DeviceAttributeApi {
 public:
  DeviceAttributeApiImpl();
  ~DeviceAttributeApiImpl() override;

  void ReportNotAffiliatedError(
      base::OnceCallback<void(blink::mojom::DeviceAttributeResultPtr)> callback)
      override;
  void ReportNotAllowedError(
      base::OnceCallback<void(blink::mojom::DeviceAttributeResultPtr)> callback)
      override;
  void GetDirectoryId(
      blink::mojom::DeviceAPIService::GetDirectoryIdCallback callback) override;
  void GetHostname(
      blink::mojom::DeviceAPIService::GetHostnameCallback callback) override;
  void GetSerialNumber(blink::mojom::DeviceAPIService::GetSerialNumberCallback
                           callback) override;
  void GetAnnotatedAssetId(
      blink::mojom::DeviceAPIService::GetAnnotatedAssetIdCallback callback)
      override;
  void GetAnnotatedLocation(
      blink::mojom::DeviceAPIService::GetAnnotatedLocationCallback callback)
      override;
};

#endif  // CHROME_BROWSER_DEVICE_API_DEVICE_ATTRIBUTE_API_H_
