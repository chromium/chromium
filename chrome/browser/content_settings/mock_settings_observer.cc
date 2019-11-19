// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/mock_settings_observer.h"

#include "chrome/browser/chrome_notification_types.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "url/gurl.h"

MockSettingsObserver::MockSettingsObserver(HostContentSettingsMap* map)
    : map_(map) {
  observer_.Add(map_);
}

MockSettingsObserver::~MockSettingsObserver() {}

void MockSettingsObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  const ContentSettingsDetails details(
      primary_pattern, secondary_pattern, content_type, resource_identifier);
  OnContentSettingsChanged(map_,
                           details.type(),
                           details.update_all_types(),
                           details.primary_pattern(),
                           details.secondary_pattern(),
                           details.update_all());
  // This checks that calling a Get function from an observer doesn't
  // deadlock.
  GURL url("http://random-hostname.com/");
  map_->GetContentSetting(url, url, ContentSettingsType::COOKIES,
                          std::string());
}
