// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api_lacros.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_device_attributes.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace {

const char kUnsupportedByAsh[] = "Not implemented.";
const char kUnsupportedProfile[] = "Not available.";

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or empty string on success. |context| is the browser context in which
// the extension is hosted.
std::string ValidateCrosapi(content::BrowserContext* context) {
  if (!chromeos::LacrosChromeServiceImpl::Get()
           ->IsAvailable<crosapi::mojom::DeviceAttributes>()) {
    return kUnsupportedByAsh;
  }

  // These APIs are used in security-sensitive contexts. We need to ensure that
  // the user for ash is the same as the user for lacros. We do this by
  // restricting the API to the default profile, which is guaranteed to be the
  // same user.
  if (!Profile::FromBrowserContext(context)->IsMainProfile())
    return kUnsupportedProfile;

  return "";
}

}  // namespace

namespace extensions {

EnterpriseDeviceAttributesBase::~EnterpriseDeviceAttributesBase() = default;

void EnterpriseDeviceAttributesBase::OnCrosapiResult(
    crosapi::mojom::DeviceAttributesStringResultPtr result) {
  using Result = crosapi::mojom::DeviceAttributesStringResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      // We intentionally drop the error message here because the extension API
      // is expected to return "" on validation error.
      OnResult("");
      return;
    case Result::Tag::CONTENTS:
      OnResult(result->get_contents());
      return;
  }
}

EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::
    EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() = default;

EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::
    ~EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::Run() {
  std::string error = ValidateCrosapi(browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::OnCrosapiResult,
      this);

  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDirectoryDeviceId(std::move(cb));
  return RespondLater();
}

void EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::OnResult(
    const std::string& result) {
  Respond(ArgumentList(
      api::enterprise_device_attributes::GetDirectoryDeviceId::Results::Create(
          result)));
}

EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::
    EnterpriseDeviceAttributesGetDeviceSerialNumberFunction() = default;

EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::
    ~EnterpriseDeviceAttributesGetDeviceSerialNumberFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::Run() {
  std::string error = ValidateCrosapi(browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::OnCrosapiResult,
      this);

  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDeviceSerialNumber(std::move(cb));
  return RespondLater();
}

void EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::OnResult(
    const std::string& result) {
  Respond(ArgumentList(
      api::enterprise_device_attributes::GetDeviceSerialNumber::Results::Create(
          result)));
}

EnterpriseDeviceAttributesGetDeviceAssetIdFunction::
    EnterpriseDeviceAttributesGetDeviceAssetIdFunction() = default;

EnterpriseDeviceAttributesGetDeviceAssetIdFunction::
    ~EnterpriseDeviceAttributesGetDeviceAssetIdFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAssetIdFunction::Run() {
  std::string error = ValidateCrosapi(browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDeviceAssetIdFunction::OnCrosapiResult,
      this);

  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDeviceAssetId(std::move(cb));
  return RespondLater();
}

void EnterpriseDeviceAttributesGetDeviceAssetIdFunction::OnResult(
    const std::string& result) {
  Respond(ArgumentList(
      api::enterprise_device_attributes::GetDeviceAssetId::Results::Create(
          result)));
}

EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::
    EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction() = default;

EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::
    ~EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::Run() {
  std::string error = ValidateCrosapi(browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::
          OnCrosapiResult,
      this);

  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDeviceAnnotatedLocation(std::move(cb));
  return RespondLater();
}

void EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::OnResult(
    const std::string& result) {
  Respond(
      ArgumentList(api::enterprise_device_attributes::
                       GetDeviceAnnotatedLocation::Results::Create(result)));
}

EnterpriseDeviceAttributesGetDeviceHostnameFunction::
    EnterpriseDeviceAttributesGetDeviceHostnameFunction() = default;

EnterpriseDeviceAttributesGetDeviceHostnameFunction::
    ~EnterpriseDeviceAttributesGetDeviceHostnameFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceHostnameFunction::Run() {
  std::string error = ValidateCrosapi(browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDeviceHostnameFunction::OnCrosapiResult,
      this);

  chromeos::LacrosChromeServiceImpl::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      ->GetDeviceHostname(std::move(cb));
  return RespondLater();
}

void EnterpriseDeviceAttributesGetDeviceHostnameFunction::OnResult(
    const std::string& result) {
  Respond(ArgumentList(
      api::enterprise_device_attributes::GetDeviceHostname::Results::Create(
          result)));
}

}  // namespace extensions
