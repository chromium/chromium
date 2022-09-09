// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_MOCK_SETTINGS_OBSERVER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_MOCK_SETTINGS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gmock/include/gmock/gmock.h"

class ContentSettingsPattern;

class MockSettingsObserver : public content_settings::Observer {
 public:
  explicit MockSettingsObserver(HostContentSettingsMap* map);

  MockSettingsObserver(const MockSettingsObserver&) = delete;
  MockSettingsObserver& operator=(const MockSettingsObserver&) = delete;

  ~MockSettingsObserver() override;

  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  MOCK_METHOD6(OnContentSettingsChanged,
               void(HostContentSettingsMap*,
                    ContentSettingsType,
                    bool,
                    const ContentSettingsPattern&,
                    const ContentSettingsPattern&,
                    bool));

 private:
  // The map that this Observer is watching.
  raw_ptr<HostContentSettingsMap> map_;

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_MOCK_SETTINGS_OBSERVER_H_
