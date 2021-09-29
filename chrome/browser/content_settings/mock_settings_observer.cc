// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/mock_settings_observer.h"

#include "chrome/browser/chrome_notification_types.h"
#include "url/gurl.h"

MockSettingsObserver::MockSettingsObserver(HostContentSettingsMap* map)
    : map_(map) {
  observation_.Observe(map_);
}

MockSettingsObserver::~MockSettingsObserver() = default;

void MockSettingsObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  bool all_types = content_type == ContentSettingsType::DEFAULT;
  bool all_hosts =
      primary_pattern.MatchesAllHosts() && secondary_pattern.MatchesAllHosts();
  OnContentSettingsChanged(map_, content_type, all_types, primary_pattern,
                           secondary_pattern, all_hosts);
  // This checks that calling a Get function from an observer doesn't
  // deadlock.
  GURL url("http://random-hostname.com/");
  map_->GetContentSetting(url, url, ContentSettingsType::COOKIES);
}
