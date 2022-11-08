// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_live_state_decorator_delegate_impl.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

PageLiveStateDelegateImpl::~PageLiveStateDelegateImpl() = default;

// static
base::SequenceBound<PageLiveStateDecorator::Delegate>
PageLiveStateDelegateImpl::Create() {
  return base::SequenceBound<PageLiveStateDelegateImpl>(
      content::GetUIThreadTaskRunner({}));
}

std::map<ContentSettingsType, ContentSetting>
PageLiveStateDelegateImpl::GetContentSettingsForUrl(
    content::WebContents* web_contents,
    const GURL& url) {
  ContentSetting setting =
      permissions::PermissionsClient::Get()
          ->GetSettingsMap(web_contents->GetBrowserContext())
          ->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS);

  return {{ContentSettingsType::NOTIFICATIONS, setting}};
}

}  // namespace performance_manager
