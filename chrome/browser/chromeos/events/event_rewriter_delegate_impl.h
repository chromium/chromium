// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_DELEGATE_IMPL_H_

#include "base/macros.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/wm/public/activation_client.h"

class PrefService;

namespace ash {
class DeprecationNotificationController;
}

namespace chromeos {

class EventRewriterDelegateImpl : public ui::EventRewriterChromeOS::Delegate {
 public:
  explicit EventRewriterDelegateImpl(wm::ActivationClient* activation_client);
  EventRewriterDelegateImpl(
      wm::ActivationClient* activation_client,
      std::unique_ptr<ash::DeprecationNotificationController>
          deprecation_controller);
  ~EventRewriterDelegateImpl() override;

  void set_pref_service_for_testing(const PrefService* pref_service) {
    pref_service_for_testing_ = pref_service;
  }

  // ui::EventRewriterChromeOS::Delegate:
  bool RewriteModifierKeys() override;
  bool GetKeyboardRemappedPrefValue(const std::string& pref_name,
                                    int* result) const override;
  bool TopRowKeysAreFunctionKeys() const override;
  bool IsExtensionCommandRegistered(ui::KeyboardCode key_code,
                                    int flags) const override;
  bool IsSearchKeyAcceleratorReserved() const override;
  bool NotifyDeprecatedRightClickRewrite() override;
  bool NotifyDeprecatedFKeyRewrite() override;
  bool NotifyDeprecatedAltBasedKeyRewrite(ui::KeyboardCode key_code) override;

 private:
  const PrefService* GetPrefService() const;

  const PrefService* pref_service_for_testing_;

  wm::ActivationClient* activation_client_;

  // Handles showing notifications when deprecated event rewrites occur.
  std::unique_ptr<ash::DeprecationNotificationController>
      deprecation_controller_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterDelegateImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_DELEGATE_IMPL_H_
