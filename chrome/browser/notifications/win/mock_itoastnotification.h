// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_MOCK_ITOASTNOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_MOCK_ITOASTNOTIFICATION_H_

#include <windows.ui.notifications.h>
#include <wrl/implements.h>

#include "base/macros.h"
#include "base/strings/string16.h"

class MockIToastNotification
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::UI::Notifications::IToastNotification,
          ABI::Windows::UI::Notifications::IToastNotification2> {
 public:
  explicit MockIToastNotification(const base::string16& xml,
                                  const base::string16& tag);
  ~MockIToastNotification() override = default;

  // ABI::Windows::UI::Notifications::IToastNotification implementation:
  HRESULT STDMETHODCALLTYPE
  get_Content(ABI::Windows::Data::Xml::Dom::IXmlDocument** value) override;
  HRESULT STDMETHODCALLTYPE put_ExpirationTime(
      __FIReference_1_Windows__CFoundation__CDateTime* value) override;
  HRESULT STDMETHODCALLTYPE get_ExpirationTime(
      __FIReference_1_Windows__CFoundation__CDateTime** value) override;
  HRESULT STDMETHODCALLTYPE add_Dismissed(
      __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_Windows__CUI__CNotifications__CToastDismissedEventArgs*
          handler,
      EventRegistrationToken* cookie) override;
  HRESULT STDMETHODCALLTYPE
  remove_Dismissed(EventRegistrationToken cookie) override;
  HRESULT STDMETHODCALLTYPE add_Activated(
      __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_IInspectable*
          handler,
      EventRegistrationToken* cookie) override;
  HRESULT STDMETHODCALLTYPE
  remove_Activated(EventRegistrationToken cookie) override;
  HRESULT STDMETHODCALLTYPE add_Failed(
      __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_Windows__CUI__CNotifications__CToastFailedEventArgs*
          handler,
      EventRegistrationToken* token) override;
  HRESULT STDMETHODCALLTYPE
  remove_Failed(EventRegistrationToken token) override;

  // ABI::Windows::UI::Notifications::IToastNotification2 implementation:
  HRESULT STDMETHODCALLTYPE put_Tag(HSTRING value) override;
  HRESULT STDMETHODCALLTYPE get_Tag(HSTRING* value) override;
  HRESULT STDMETHODCALLTYPE put_Group(HSTRING value) override;
  HRESULT STDMETHODCALLTYPE get_Group(HSTRING* value) override;
  HRESULT STDMETHODCALLTYPE put_SuppressPopup(boolean value) override;
  HRESULT STDMETHODCALLTYPE get_SuppressPopup(boolean* value) override;

 private:
  base::string16 xml_;

  base::string16 group_;
  base::string16 tag_;

  DISALLOW_COPY_AND_ASSIGN(MockIToastNotification);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_MOCK_ITOASTNOTIFICATION_H_
