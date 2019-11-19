// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CHOOSER_CONTEXT_BASE_MOCK_PERMISSION_OBSERVER_H_
#define CHROME_BROWSER_PERMISSIONS_CHOOSER_CONTEXT_BASE_MOCK_PERMISSION_OBSERVER_H_

#include "chrome/browser/permissions/chooser_context_base.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

class MockPermissionObserver : public ChooserContextBase::PermissionObserver {
 public:
  MockPermissionObserver();
  ~MockPermissionObserver() override;

  MOCK_METHOD2(OnChooserObjectPermissionChanged,
               void(ContentSettingsType guard_content_settings_type,
                    ContentSettingsType data_content_settings_type));
  MOCK_METHOD2(OnPermissionRevoked,
               void(const url::Origin& requesting_origin,
                    const url::Origin& embedding_origin));
};

#endif  // CHROME_BROWSER_PERMISSIONS_CHOOSER_CONTEXT_BASE_MOCK_PERMISSION_OBSERVER_H_
