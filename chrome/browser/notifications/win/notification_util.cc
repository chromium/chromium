// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/notification_util.h"

#include <wrl/client.h>

#include "base/logging.h"
#include "base/win/scoped_hstring.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/notifications/win/notification_metrics.h"
#include "chrome/browser/notifications/win/notification_template_builder.h"

namespace mswr = Microsoft::WRL;
namespace winfoundtn = ABI::Windows::Foundation;
namespace winui = ABI::Windows::UI;
namespace winxml = ABI::Windows::Data::Xml;

using notifications_uma::GetNotificationLaunchIdStatus;
using base::win::ScopedHString;

NotificationLaunchId GetNotificationLaunchId(
    winui::Notifications::IToastNotification* notification) {
  mswr::ComPtr<winxml::Dom::IXmlDocument> document;
  HRESULT hr = notification->get_Content(&document);
  if (FAILED(hr)) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kNotificationGetContentFailed);
    DLOG(ERROR) << "Failed to get XML document";
    return NotificationLaunchId();
  }

  ScopedHString tag = ScopedHString::Create(kNotificationToastElement);
  mswr::ComPtr<winxml::Dom::IXmlNodeList> elements;
  hr = document->GetElementsByTagName(tag.get(), &elements);
  if (FAILED(hr)) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kGetElementsByTagFailed);
    DLOG(ERROR) << "Failed to get <toast> elements from document";
    return NotificationLaunchId();
  }

  UINT32 length;
  hr = elements->get_Length(&length);
  if (FAILED(hr) || length == 0) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kMissingToastElementInDoc);
    DLOG(ERROR) << "No <toast> elements in document.";
    return NotificationLaunchId();
  }

  mswr::ComPtr<winxml::Dom::IXmlNode> node;
  hr = elements->Item(0, &node);
  if (FAILED(hr)) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kItemAtFailed);
    DLOG(ERROR) << "Failed to get first <toast> element";
    return NotificationLaunchId();
  }

  mswr::ComPtr<winxml::Dom::IXmlNamedNodeMap> attributes;
  hr = node->get_Attributes(&attributes);
  if (FAILED(hr)) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kGetAttributesFailed);
    DLOG(ERROR) << "Failed to get attributes of <toast>";
    return NotificationLaunchId();
  }

  mswr::ComPtr<winxml::Dom::IXmlNode> leaf;
  ScopedHString id = ScopedHString::Create(kNotificationLaunchAttribute);
  hr = attributes->GetNamedItem(id.get(), &leaf);
  if (FAILED(hr)) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kGetNamedItemFailed);
    DLOG(ERROR) << "Failed to get launch attribute of <toast>";
    return NotificationLaunchId();
  }

  if (!leaf) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kGetNamedItemNull);
    DLOG(ERROR) << "GetNamedItem returned null querying for 'launch' attribute";
    return NotificationLaunchId();
  }

  mswr::ComPtr<winxml::Dom::IXmlNode> child;
  hr = leaf->get_FirstChild(&child);
  if (FAILED(hr)) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kGetFirstChildFailed);
    DLOG(ERROR) << "Failed to get content of launch attribute";
    return NotificationLaunchId();
  }

  if (!child) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kGetFirstChildNull);
    DLOG(ERROR) << "Launch attribute is a null node";
    return NotificationLaunchId();
  }

  mswr::ComPtr<IInspectable> inspectable;
  hr = child->get_NodeValue(&inspectable);
  if (FAILED(hr)) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kGetNodeValueFailed);
    DLOG(ERROR) << "Failed to get node value of launch attribute";
    return NotificationLaunchId();
  }

  mswr::ComPtr<winfoundtn::IPropertyValue> property_value;
  hr = inspectable.As<winfoundtn::IPropertyValue>(&property_value);
  if (FAILED(hr)) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kConversionToPropValueFailed);
    DLOG(ERROR) << "Failed to convert node value of launch attribute";
    return NotificationLaunchId();
  }

  HSTRING value_hstring;
  hr = property_value->GetString(&value_hstring);
  if (FAILED(hr)) {
    LogGetNotificationLaunchIdStatus(
        GetNotificationLaunchIdStatus::kGetStringFailed);
    DLOG(ERROR) << "Failed to get string for launch attribute";
    return NotificationLaunchId();
  }

  LogGetNotificationLaunchIdStatus(GetNotificationLaunchIdStatus::kSuccess);

  ScopedHString value(value_hstring);
  return NotificationLaunchId(value.GetAsUTF8());
}
