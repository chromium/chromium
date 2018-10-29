// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/mock_itoastnotification.h"

#include <wrl/client.h>

#include "base/strings/string_piece.h"
#include "base/win/scoped_hstring.h"

namespace mswr = Microsoft::WRL;
namespace winui = ABI::Windows::UI;
namespace winxml = ABI::Windows::Data::Xml;

MockIToastNotification::MockIToastNotification(const base::string16& xml,
                                               const base::string16& tag)
    : xml_(xml), group_(L"Notifications"), tag_(tag) {}

HRESULT MockIToastNotification::get_Content(winxml::Dom::IXmlDocument** value) {
  mswr::ComPtr<winxml::Dom::IXmlDocumentIO> xml_document_io;
  base::win::ScopedHString id = base::win::ScopedHString::Create(
      RuntimeClass_Windows_Data_Xml_Dom_XmlDocument);
  HRESULT hr = Windows::Foundation::ActivateInstance(
      id.get(), xml_document_io.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to instantiate XMLDocumentIO " << hr;
    return hr;
  }

  base::win::ScopedHString xml = base::win::ScopedHString::Create(xml_);
  hr = xml_document_io->LoadXml(xml.get());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to load XML " << hr;
    return hr;
  }

  mswr::ComPtr<winxml::Dom::IXmlDocument> xml_document;
  hr = xml_document_io.CopyTo(xml_document.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to copy to XMLDoc " << hr;
    return hr;
  }

  *value = xml_document.Detach();
  return S_OK;
}

HRESULT MockIToastNotification::put_ExpirationTime(
    __FIReference_1_Windows__CFoundation__CDateTime* value) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::get_ExpirationTime(
    __FIReference_1_Windows__CFoundation__CDateTime** value) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::add_Dismissed(
    __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_Windows__CUI__CNotifications__CToastDismissedEventArgs*
        handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::remove_Dismissed(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::add_Activated(
    __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_IInspectable*
        handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::remove_Activated(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::add_Failed(
    __FITypedEventHandler_2_Windows__CUI__CNotifications__CToastNotification_Windows__CUI__CNotifications__CToastFailedEventArgs*
        handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::remove_Failed(EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::put_Tag(HSTRING value) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::get_Tag(HSTRING* value) {
  base::win::ScopedHString tag = base::win::ScopedHString::Create(tag_);
  *value = tag.release();
  return S_OK;
}

HRESULT MockIToastNotification::put_Group(HSTRING value) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::get_Group(HSTRING* value) {
  base::win::ScopedHString group = base::win::ScopedHString::Create(group_);
  *value = group.release();
  return S_OK;
}

HRESULT MockIToastNotification::put_SuppressPopup(boolean value) {
  return E_NOTIMPL;
}

HRESULT MockIToastNotification::get_SuppressPopup(boolean* value) {
  return E_NOTIMPL;
}
