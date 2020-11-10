// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MOCK_OBSERVER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MOCK_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content_settings {

class MockObserver : public Observer {
 public:
  MockObserver();
  ~MockObserver() override;

  MOCK_METHOD3(OnContentSettingChanged,
               void(const ContentSettingsPattern& primary_pattern,
                    const ContentSettingsPattern& secondary_pattern,
                    ContentSettingsType content_type));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MOCK_OBSERVER_H_
