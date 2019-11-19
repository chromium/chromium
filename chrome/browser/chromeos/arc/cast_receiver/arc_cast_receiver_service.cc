// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/cast_receiver/arc_cast_receiver_service.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace arc {
namespace {

// A callback for CastReceiverInstance calls that ignores the result passed
// to it. We use this for calls made from a preferences observer since there
// is noone to pass that result to now.
void OnResultReceivedIgnoreResult(mojom::CastReceiverInstance::Result) {}

// Singleton factory for ArcCastReceiverService.
class ArcCastReceiverServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcCastReceiverService,
          ArcCastReceiverServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcCastReceiverServiceFactory";

  static ArcCastReceiverServiceFactory* GetInstance() {
    return base::Singleton<ArcCastReceiverServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcCastReceiverServiceFactory>;
  ArcCastReceiverServiceFactory() = default;
  ~ArcCastReceiverServiceFactory() override = default;
};

}  // namespace

// static
ArcCastReceiverService* ArcCastReceiverService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcCastReceiverServiceFactory::GetForBrowserContext(context);
}

ArcCastReceiverService::ArcCastReceiverService(content::BrowserContext* context,
                                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->cast_receiver()->AddObserver(this);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(
      Profile::FromBrowserContext(context)->GetPrefs());
  // Observe prefs for the Cast Receiver. We can use base::Unretained() here
  // because we own |pref_change_registrar_|.
  pref_change_registrar_->Add(
      prefs::kCastReceiverEnabled,
      base::Bind(&ArcCastReceiverService::OnCastReceiverEnabledChanged,
                 base::Unretained(this)));

  receiver_name_subscription_ =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kCastReceiverName,
          base::Bind(&ArcCastReceiverService::OnCastReceiverNameChanged,
                     base::Unretained(this)));
}

ArcCastReceiverService::~ArcCastReceiverService() {
  arc_bridge_service_->cast_receiver()->RemoveObserver(this);
}

void ArcCastReceiverService::OnConnectionReady() {
  // Push all existing preferences to the Cast Receiver. Always end with
  // the preference for enabling the receiver so that it does not show up
  // briefly with the wrong settings (e.g. its name).
  OnCastReceiverNameChanged();
  // Now sets whether the receiver is enabled or not, which will make it
  // discoverable if needed.
  OnCastReceiverEnabledChanged();
}

void ArcCastReceiverService::OnCastReceiverEnabledChanged() const {
  mojom::CastReceiverInstance* cast_receiver_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->cast_receiver(),
                                  SetEnabled);
  if (!cast_receiver_instance)
    return;
  cast_receiver_instance->SetEnabled(
      pref_change_registrar_->prefs()->GetBoolean(prefs::kCastReceiverEnabled),
      base::BindOnce(&OnResultReceivedIgnoreResult));
}

void ArcCastReceiverService::OnCastReceiverNameChanged() const {
  mojom::CastReceiverInstance* cast_receiver_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->cast_receiver(),
                                  SetName);
  if (!cast_receiver_instance)
    return;
  std::string name;
  if (!chromeos::CrosSettings::Get()->GetString(chromeos::kCastReceiverName,
                                                &name) ||
      name.empty()) {
    return;
  }
  cast_receiver_instance->SetName(
      name, base::BindOnce(&OnResultReceivedIgnoreResult));
}

}  // namespace arc
