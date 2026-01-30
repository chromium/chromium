// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_CRYPTOHOME_KEY_DELEGATE_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_CRYPTOHOME_KEY_DELEGATE_SERVICE_PROVIDER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"

class PrefService;

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace ash {

// Provider for the org.chromium.CryptohomeKeyDelegateInterface service
// implementation.
//
// This service is called by the cryptohomed daemon for operations related to
// user protection keys. See the interface definition in the Chrome OS repo in
// src/platform2/cryptohome/dbus_bindings/
//   org.chromium.CryptohomeKeyDelegateInterface.xml .
class CryptohomeKeyDelegateServiceProvider final
    : public CrosDBusService::ServiceProviderInterface {
 public:
  // `local_state` must be non-null and must outlive `this`.
  explicit CryptohomeKeyDelegateServiceProvider(PrefService* local_state);

  CryptohomeKeyDelegateServiceProvider(
      const CryptohomeKeyDelegateServiceProvider&) = delete;
  CryptohomeKeyDelegateServiceProvider& operator=(
      const CryptohomeKeyDelegateServiceProvider&) = delete;

  ~CryptohomeKeyDelegateServiceProvider() override;

  // CrosDBusService::ServiceProviderInterface overrides:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  // Implements the "ChallengeKey" D-Bus method.
  void HandleChallengeKey(dbus::MethodCall* method_call,
                          dbus::ExportedObject::ResponseSender response_sender);

  const raw_ref<PrefService> local_state_;

  // Must be the last member.
  base::WeakPtrFactory<CryptohomeKeyDelegateServiceProvider> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_CRYPTOHOME_KEY_DELEGATE_SERVICE_PROVIDER_H_
