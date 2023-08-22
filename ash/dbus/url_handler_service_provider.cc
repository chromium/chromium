// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/url_handler_service_provider.h"

#include <utility>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "extensions/common/constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace ash {
namespace {
constexpr const char* kUrlSchemes[] = {url::kDataScheme,  url::kFileScheme,
                                       url::kFtpScheme,   url::kHttpScheme,
                                       url::kHttpsScheme, url::kMailToScheme};

// Called from ExportedObject when OpenUrl() is exported as a D-Bus method or
// failed to be exported.
void OnExported(const std::string& interface_name,
                const std::string& method_name,
                bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

}  // namespace

UrlHandlerServiceProvider::UrlHandlerServiceProvider()
    : allowed_url_schemes_(std::cbegin(kUrlSchemes), std::cend(kUrlSchemes)) {}

UrlHandlerServiceProvider::~UrlHandlerServiceProvider() = default;

void UrlHandlerServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      chromeos::kUrlHandlerServiceInterface,
      chromeos::kUrlHandlerServiceOpenUrlMethod,
      base::BindRepeating(&UrlHandlerServiceProvider::OpenUrl,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&OnExported));
}

bool UrlHandlerServiceProvider::UrlAllowed(const GURL& gurl) const {
  return gurl.is_valid() && base::Contains(allowed_url_schemes_, gurl.scheme());
}

void UrlHandlerServiceProvider::OpenUrl(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string url;
  if (!reader.PopString(&url)) {
    LOG(ERROR) << "Method call lacks URL: " << method_call->ToString();
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(
            method_call, DBUS_ERROR_INVALID_ARGS, "No URL string arg"));
    return;
  }

  VLOG(1) << "Got request to open url";

  const GURL gurl(url);
  if (!UrlAllowed(gurl)) {
    VLOG(1) << "Url is not allowed";
    std::move(response_sender)
        .Run(dbus::ErrorResponse::FromMethodCall(method_call, DBUS_ERROR_FAILED,
                                                 "Invalid URL"));
    return;
  }

  VLOG(1) << "Opening url now";

  NewWindowDelegate::GetPrimary()->OpenUrl(
      gurl, NewWindowDelegate::OpenUrlFrom::kUnspecified,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  std::move(response_sender).Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace ash
