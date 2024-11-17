// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/device_attributes_ash.h"

namespace {

crosapi::mojom::DeviceAttributes* GetDeviceAttributesApi() {
  return crosapi::CrosapiManager::Get()->crosapi_ash()->device_attributes_ash();
}

}  // namespace

namespace extensions {

void EnterpriseDeviceAttributesBase::OnCrosapiResult(
    crosapi::mojom::DeviceAttributesStringResultPtr result) {
  using Result = crosapi::mojom::DeviceAttributesStringResult;
  switch (result->which()) {
    case Result::Tag::kErrorMessage:
      // We intentionally drop the error message here because the extension API
      // is expected to return "" on validation error.
      Respond(WithArguments(""));
      return;
    case Result::Tag::kContents:
      Respond(WithArguments(result->get_contents()));
      return;
  }
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::OnCrosapiResult,
      this);

  GetDeviceAttributesApi()->GetDirectoryDeviceId(std::move(cb));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::OnCrosapiResult,
      this);

  GetDeviceAttributesApi()->GetDeviceSerialNumber(std::move(cb));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAssetIdFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDeviceAssetIdFunction::OnCrosapiResult,
      this);

  GetDeviceAttributesApi()->GetDeviceAssetId(std::move(cb));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::
          OnCrosapiResult,
      this);

  GetDeviceAttributesApi()->GetDeviceAnnotatedLocation(std::move(cb));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceHostnameFunction::Run() {
  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDeviceHostnameFunction::OnCrosapiResult,
      this);

  GetDeviceAttributesApi()->GetDeviceHostname(std::move(cb));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

}  // namespace extensions
