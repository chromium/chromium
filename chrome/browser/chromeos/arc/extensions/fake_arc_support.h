// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_EXTENSIONS_FAKE_ARC_SUPPORT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_EXTENSIONS_FAKE_ARC_SUPPORT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace arc {

// Fake implementation of ARC support Chrome App for testing.
class FakeArcSupport : public extensions::NativeMessageHost::Client {
 public:
  // Observer to get arc support related notifications.
  class Observer {
   public:
    // Called when ui_page() changes, with the new page as argument.
    virtual void OnPageChanged(ArcSupportHost::UIPage page) {}

   protected:
    virtual ~Observer() = default;
  };

  explicit FakeArcSupport(ArcSupportHost* support_host);
  ~FakeArcSupport() override;

  // Emulates to open ARC support Chrome app, and connect message host to
  // ARC support host.
  void Open(Profile* profile);

  // Emulates clicking Close button.
  void Close();

  // Authentication page emulation for Active Directory.
  void EmulateAuthSuccess();
  void EmulateAuthFailure(const std::string& error_msg);

  // Emulates clicking Agree button on the fake terms of service page.
  void ClickAgreeButton();

  // Emulates clicking Cancel button on the fake terms of service page.
  void ClickCancelButton();

  // Emulates clicking Cancel button on the fake Active Directory auth page.
  void ClickAdAuthCancelButton();

  // Error page emulation.
  void ClickRetryButton();
  void ClickSendFeedbackButton();

  bool metrics_mode() const { return metrics_mode_; }
  bool backup_and_restore_managed() const {
    return backup_and_restore_managed_;
  }
  bool backup_and_restore_mode() const { return backup_and_restore_mode_; }
  bool location_service_managed() const { return location_service_managed_; }
  bool location_service_mode() const { return location_service_mode_; }
  const std::string& tos_content() const { return tos_content_; }
  bool tos_shown() const { return tos_shown_; }

  // Emulates checking preference box.
  void set_metrics_mode(bool mode) { metrics_mode_ = mode; }
  void set_backup_and_restore_mode(bool mode) {
    backup_and_restore_mode_ = mode;
  }
  void set_location_service_mode(bool mode) { location_service_mode_ = mode; }

  // Allows control of whether some preferences are managed by policy.
  void set_backup_and_restore_managed(bool managed) {
    backup_and_restore_managed_ = managed;
  }
  void set_location_service_managed(bool managed) {
    location_service_managed_ = managed;
  }

  // Allows emulation of the ToS display.
  void set_tos_content(const std::string& tos_content) {
    tos_content_ = tos_content;
  }
  void set_tos_shown(bool shown) { tos_shown_ = shown; }

  // Emulate setting the Active Directory auth federation URL.
  void set_active_directory_auth_federation_url(
      const std::string& federation_url) {
    active_directory_auth_federation_url_ = federation_url;
  }

  const std::string& active_directory_auth_federation_url() const {
    return active_directory_auth_federation_url_;
  }

  // Emulate setting the Active Directory DM server URL prefix.
  void set_active_directory_auth_device_management_url_prefix(
      const std::string& device_management_url_prefix) {
    active_directory_auth_device_management_url_prefix_ =
        device_management_url_prefix;
  }

  const std::string& active_directory_auth_device_management_url_prefix()
      const {
    return active_directory_auth_device_management_url_prefix_;
  }

  // Returns the current page.
  ArcSupportHost::UIPage ui_page() const { return ui_page_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer);

 private:
  void UnsetMessageHost();

  // extensions::NativeMessageHost::Client:
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

  ArcSupportHost* const support_host_;

  std::unique_ptr<extensions::NativeMessageHost> native_message_host_;
  ArcSupportHost::UIPage ui_page_ = ArcSupportHost::UIPage::NO_PAGE;
  bool metrics_mode_ = false;
  bool backup_and_restore_managed_ = false;
  bool backup_and_restore_mode_ = false;
  bool location_service_managed_ = false;
  bool location_service_mode_ = false;
  std::string tos_content_;
  bool tos_shown_ = false;
  std::string active_directory_auth_federation_url_;
  std::string active_directory_auth_device_management_url_prefix_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<FakeArcSupport> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeArcSupport);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_EXTENSIONS_FAKE_ARC_SUPPORT_H_
