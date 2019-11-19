// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/extensions/fake_arc_support.h"

#include <string>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/extensions/arc_support_message_host.h"
#include "chrome/browser/profiles/profile.h"

namespace {

void SerializeAndSend(extensions::NativeMessageHost* native_message_host,
                      const base::DictionaryValue& message) {
  DCHECK(native_message_host);
  std::string message_string;
  if (!base::JSONWriter::Write(message, &message_string)) {
    NOTREACHED();
    return;
  }
  native_message_host->OnMessage(message_string);
}

}  // namespace

namespace arc {

FakeArcSupport::FakeArcSupport(ArcSupportHost* support_host)
    : support_host_(support_host) {
  DCHECK(support_host_);
  support_host_->SetRequestOpenAppCallbackForTesting(
      base::Bind(&FakeArcSupport::Open, weak_ptr_factory_.GetWeakPtr()));
}

FakeArcSupport::~FakeArcSupport() {
  // Ensure that message host is disconnected.
  if (!native_message_host_)
    return;
  UnsetMessageHost();
}

void FakeArcSupport::Open(Profile* profile) {
  DCHECK(!native_message_host_);
  native_message_host_ = ArcSupportMessageHost::Create(profile);
  native_message_host_->Start(this);
  support_host_->SetMessageHost(
      static_cast<ArcSupportMessageHost*>(native_message_host_.get()));
}

void FakeArcSupport::Close() {
  DCHECK(native_message_host_);
  native_message_host_->OnMessage("{\"event\": \"onWindowClosed\"}");
  UnsetMessageHost();
}

void FakeArcSupport::EmulateAuthSuccess() {
  DCHECK_EQ(ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH, ui_page_);
  base::DictionaryValue message;
  message.SetString("event", "onAuthSucceeded");
  SerializeAndSend(native_message_host_.get(), message);
}

void FakeArcSupport::EmulateAuthFailure(const std::string& error_msg) {
  DCHECK(native_message_host_);
  DCHECK_EQ(ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH, ui_page_);
  base::DictionaryValue message;
  message.SetString("event", "onAuthFailed");
  message.SetString("errorMessage", error_msg);
  SerializeAndSend(native_message_host_.get(), message);
}

void FakeArcSupport::ClickAgreeButton() {
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::TERMS);
  base::DictionaryValue message;
  message.SetString("event", "onAgreed");
  message.SetString("tosContent", tos_content_);
  message.SetBoolean("tosShown", tos_shown_);
  message.SetBoolean("isMetricsEnabled", metrics_mode_);
  message.SetBoolean("isBackupRestoreEnabled", backup_and_restore_mode_);
  message.SetBoolean("isBackupRestoreManaged", backup_and_restore_managed_);
  message.SetBoolean("isLocationServiceEnabled", location_service_mode_);
  message.SetBoolean("isLocationServiceManaged", location_service_managed_);
  SerializeAndSend(native_message_host_.get(), message);
}

void FakeArcSupport::ClickCancelButton() {
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::TERMS);
  base::DictionaryValue message;
  message.SetString("event", "onCanceled");
  message.SetString("tosContent", tos_content_);
  message.SetBoolean("tosShown", tos_shown_);
  message.SetBoolean("isMetricsEnabled", metrics_mode_);
  message.SetBoolean("isBackupRestoreEnabled", backup_and_restore_mode_);
  message.SetBoolean("isBackupRestoreManaged", backup_and_restore_managed_);
  message.SetBoolean("isLocationServiceEnabled", location_service_mode_);
  message.SetBoolean("isLocationServiceManaged", location_service_managed_);
  SerializeAndSend(native_message_host_.get(), message);
  // The cancel button closes the window.
  Close();
}

void FakeArcSupport::ClickAdAuthCancelButton() {
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH);
  // The cancel button closes the window.
  Close();
}

void FakeArcSupport::ClickRetryButton() {
  DCHECK(native_message_host_);
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::ERROR);
  native_message_host_->OnMessage("{\"event\": \"onRetryClicked\"}");
}

void FakeArcSupport::ClickSendFeedbackButton() {
  DCHECK(native_message_host_);
  DCHECK_EQ(ui_page_, ArcSupportHost::UIPage::ERROR);
  native_message_host_->OnMessage("{\"event\": \"onSendFeedbackClicked\"}");
}

void FakeArcSupport::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeArcSupport::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool FakeArcSupport::HasObserver(Observer* observer) {
  return observer_list_.HasObserver(observer);
}

void FakeArcSupport::UnsetMessageHost() {
  support_host_->UnsetMessageHost(
      static_cast<ArcSupportMessageHost*>(native_message_host_.get()));
  native_message_host_.reset();
}

void FakeArcSupport::PostMessageFromNativeHost(
    const std::string& message_string) {
  std::unique_ptr<base::DictionaryValue> message = base::DictionaryValue::From(
      base::JSONReader::ReadDeprecated(message_string));
  DCHECK(message);

  std::string action;
  if (!message->GetString("action", &action)) {
    NOTREACHED() << message_string;
    return;
  }

  ArcSupportHost::UIPage prev_ui_page = ui_page_;
  if (action == "initialize") {
    // Do nothing as emulation.
  } else if (action == "showPage") {
    std::string page;
    if (!message->GetString("page", &page)) {
      NOTREACHED() << message_string;
      return;
    }
    if (page == "terms") {
      ui_page_ = ArcSupportHost::UIPage::TERMS;
    } else if (page == "arc-loading") {
      ui_page_ = ArcSupportHost::UIPage::ARC_LOADING;
    } else if (page == "active-directory-auth") {
      ui_page_ = ArcSupportHost::UIPage::ACTIVE_DIRECTORY_AUTH;
      const base::Value* federation_url = message->FindPathOfType(
          {"options", "federationUrl"}, base::Value::Type::STRING);
      const base::Value* device_management_url_prefix = message->FindPathOfType(
          {"options", "deviceManagementUrlPrefix"}, base::Value::Type::STRING);
      if (!federation_url || !device_management_url_prefix) {
        NOTREACHED() << message_string;
        return;
      }
      active_directory_auth_federation_url_ = federation_url->GetString();
      active_directory_auth_device_management_url_prefix_ =
          device_management_url_prefix->GetString();
    } else {
      NOTREACHED() << message_string;
    }
  } else if (action == "showErrorPage") {
    ui_page_ = ArcSupportHost::UIPage::ERROR;
  } else if (action == "setMetricsMode") {
    if (!message->GetBoolean("enabled", &metrics_mode_)) {
      NOTREACHED() << message_string;
      return;
    }
  } else if (action == "setBackupAndRestoreMode") {
    if (!message->GetBoolean("enabled", &backup_and_restore_mode_)) {
      NOTREACHED() << message_string;
      return;
    }
  } else if (action == "setLocationServiceMode") {
    if (!message->GetBoolean("enabled", &location_service_mode_)) {
      NOTREACHED() << message_string;
      return;
    }
  } else if (action == "closeWindow") {
    // Do nothing as emulation.
  } else if (action == "setWindowBounds") {
    // Do nothing as emulation.
  } else {
    // Unknown or unsupported action.
    NOTREACHED() << message_string;
  }
  if (prev_ui_page != ui_page_) {
    for (auto& observer : observer_list_)
      observer.OnPageChanged(ui_page_);
  }
}

void FakeArcSupport::CloseChannel(const std::string& error_message) {
  NOTREACHED();
}

}  // namespace arc
