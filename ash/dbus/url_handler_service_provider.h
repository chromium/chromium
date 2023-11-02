// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DBUS_URL_HANDLER_SERVICE_PROVIDER_H_
#define ASH_DBUS_URL_HANDLER_SERVICE_PROVIDER_H_

#include <memory>
#include <set>
#include <string>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "url/gurl.h"

namespace dbus {
class MethodCall;
}

namespace ash {

// This class exports D-Bus methods for asking Chrome to open a URL.
//
// OpenUrl:
// % dbus-send --system --type=method_call --print-reply
//     --dest=org.chromium.UrlHandlerService
//     /org/chromium/UrlHandlerService
//     org.chromium.UrlHandlerServiceInterface.OpenUrl
//     "string:|url|"
//
// % (returns true on success, otherwise returns false)
class ASH_EXPORT UrlHandlerServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  UrlHandlerServiceProvider();

  UrlHandlerServiceProvider(const UrlHandlerServiceProvider&) = delete;
  UrlHandlerServiceProvider& operator=(const UrlHandlerServiceProvider&) =
      delete;

  ~UrlHandlerServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

  // Returns true if |gurl| is allowed to be opened in a new tab.
  // Visible for testing.
  bool UrlAllowed(const GURL& gurl) const;

 private:
  // Called on UI thread in response to a D-Bus request.
  void OpenUrl(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender);

  // Schemes that we allow to be sent via OpenUrl.
  const std::set<std::string> allowed_url_schemes_;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<UrlHandlerServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_DBUS_URL_HANDLER_SERVICE_PROVIDER_H_
