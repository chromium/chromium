// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_FAKE_ITOASTNOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_FAKE_ITOASTNOTIFICATION_H_

#include <windows.ui.notifications.h>
#include <wrl/implements.h>

#include <string>

class FakeIToastNotification
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::UI::Notifications::IToastNotification,
          ABI::Windows::UI::Notifications::IToastNotification2> {
 public:
  explicit FakeIToastNotification(const std::wstring& xml,
                                  const std::wstring& tag);
  FakeIToastNotification(const FakeIToastNotification&) = delete;
  FakeIToastNotification& operator=(const FakeIToastNotification&) = delete;
  ~FakeIToastNotification() override = default;

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
  std::wstring xml_;

  std::wstring group_;
  std::wstring tag_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_FAKE_ITOASTNOTIFICATION_H_
