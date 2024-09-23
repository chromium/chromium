// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api.h"

#include <utility>

#include "base/functional/bind.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include <optional>

#include "chrome/browser/profiles/profile.h"
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/device_attributes_ash.h"
#endif

namespace {

crosapi::mojom::DeviceAttributes* GetDeviceAttributesApi() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::DeviceAttributes>()
      .get();
#else
  return crosapi::CrosapiManager::Get()->crosapi_ash()->device_attributes_ash();
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kUnsupportedByAsh[] = "Not supported by ash.";
const char kUnsupportedProfile[] = "Not available for this profile.";

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or nullopt on success. |context| is the browser context in which the
// extension is hosted.
std::optional<std::string> ValidateCrosapi(content::BrowserContext* context) {
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::DeviceAttributes>()) {
    return kUnsupportedByAsh;
  }

  // These APIs are used in security-sensitive contexts. We need to ensure that
  // the user for ash is the same as the user for lacros. We do this by
  // restricting the API to the default profile, which is guaranteed to be the
  // same user.
  if (!Profile::FromBrowserContext(context)->IsMainProfile()) {
    return kUnsupportedProfile;
  }

  return std::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<std::string> error = ValidateCrosapi(browser_context());
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<std::string> error = ValidateCrosapi(browser_context());
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<std::string> error = ValidateCrosapi(browser_context());
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<std::string> error = ValidateCrosapi(browser_context());
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<std::string> error = ValidateCrosapi(browser_context());
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // We don't need Unretained() or WeakPtr because ExtensionFunction is
  // ref-counted.
  auto cb = base::BindOnce(
      &EnterpriseDeviceAttributesGetDeviceHostnameFunction::OnCrosapiResult,
      this);

  GetDeviceAttributesApi()->GetDeviceHostname(std::move(cb));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

}  // namespace extensions
