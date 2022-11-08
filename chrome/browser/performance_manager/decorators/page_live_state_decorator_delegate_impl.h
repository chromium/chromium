// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequence_bound.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_LIVE_STATE_DECORATOR_DELEGATE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_LIVE_STATE_DECORATOR_DELEGATE_IMPL_H_

namespace performance_manager {

// Concrete implementation of PageLiveStateDecorator::Delegate that is used in
// non-test code. This obtains the relevant content settings through
// `PermissionsClient` and currently only handles the `NOTIFICATIONS` setting.
class PageLiveStateDelegateImpl
    : public performance_manager::PageLiveStateDecorator::Delegate {
 public:
  ~PageLiveStateDelegateImpl() override;

  static base::SequenceBound<PageLiveStateDecorator::Delegate> Create();

  // Gets the content settings for `url` in the profile of `web_contents_proxy`.
  // Currently only handles the `NOTIFICATIONS` setting.
  std::map<ContentSettingsType, ContentSetting> GetContentSettingsForUrl(
      content::WebContents* web_contents,
      const GURL& url) override;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_LIVE_STATE_DECORATOR_DELEGATE_IMPL_H_
