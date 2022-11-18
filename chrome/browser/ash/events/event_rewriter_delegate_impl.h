// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EVENTS_EVENT_REWRITER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_EVENTS_EVENT_REWRITER_DELEGATE_IMPL_H_

#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/wm/public/activation_client.h"

class PrefService;

namespace ash {

class DeprecationNotificationController;

class EventRewriterDelegateImpl : public ui::EventRewriterChromeOS::Delegate {
 public:
  explicit EventRewriterDelegateImpl(wm::ActivationClient* activation_client);
  EventRewriterDelegateImpl(wm::ActivationClient* activation_client,
                            std::unique_ptr<DeprecationNotificationController>
                                deprecation_controller);

  EventRewriterDelegateImpl(const EventRewriterDelegateImpl&) = delete;
  EventRewriterDelegateImpl& operator=(const EventRewriterDelegateImpl&) =
      delete;

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
  bool NotifyDeprecatedSixPackKeyRewrite(ui::KeyboardCode key_code) override;
  void SuppressModifierKeyRewrites(bool should_suppress) override;

 private:
  const PrefService* GetPrefService() const;

  const PrefService* pref_service_for_testing_;

  wm::ActivationClient* activation_client_;

  // Handles showing notifications when deprecated event rewrites occur.
  std::unique_ptr<DeprecationNotificationController> deprecation_controller_;

  // Tracks whether modifier rewrites should be suppressed or not.
  bool suppress_modifier_key_rewrites_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EVENTS_EVENT_REWRITER_DELEGATE_IMPL_H_
