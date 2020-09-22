// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"

base::string16 CreateMacNotificationTitle(
    const message_center::Notification& notification) {
  base::string16 title;
  // Show progress percentage if available. We don't support indeterminate
  // states on macOS native notifications.
  if (notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS &&
      notification.progress() >= 0 && notification.progress() <= 100) {
    title += base::FormatPercent(notification.progress());
    title += base::UTF8ToUTF16(" - ");
  }
  title += notification.title();
  return title;
}

base::string16 CreateMacNotificationContext(
    bool is_persistent,
    const message_center::Notification& notification,
    bool requires_attribution) {
  if (!requires_attribution)
    return notification.context_message();

  // Mac OS notifications don't provide a good way to elide the domain (or tell
  // you the maximum width of the subtitle field). We have experimentally
  // determined the maximum number of characters that fit using the widest
  // possible character (m). If the domain fits in those character we show it
  // completely. Otherwise we use eTLD + 1.

  // These numbers have been obtained through experimentation on various
  // Mac OS platforms.

  constexpr size_t kMaxDomainLengthAlert = 19;
  constexpr size_t kMaxDomainLengthBanner = 28;

  size_t max_characters =
      is_persistent ? kMaxDomainLengthAlert : kMaxDomainLengthBanner;

  base::string16 origin = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(notification.origin_url()),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

  if (origin.size() <= max_characters)
    return origin;

  // Too long, use etld+1
  base::string16 etldplusone =
      base::UTF8ToUTF16(net::registry_controlled_domains::GetDomainAndRegistry(
          notification.origin_url(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));

  // localhost, raw IPs etc. are not handled by GetDomainAndRegistry.
  if (etldplusone.empty())
    return origin;

  return etldplusone;
}
