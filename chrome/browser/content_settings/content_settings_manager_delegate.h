// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MANAGER_DELEGATE_H_

#include "components/content_settings/browser/content_settings_manager_impl.h"

class ContentSettingsManagerDelegate
    : public content_settings::ContentSettingsManagerImpl::Delegate {
 public:
  ContentSettingsManagerDelegate();
  ~ContentSettingsManagerDelegate() override;

 private:
  // content_settings::ContentSettingsManagerImpl::Delegate:
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings(
      content::BrowserContext* browser_context) override;
  bool AllowStorageAccess(
      const content::GlobalRenderFrameHostToken& frame_token,
      content_settings::mojom::ContentSettingsManager::StorageType storage_type,
      const GURL& url,
      bool allowed,
      base::OnceCallback<void(bool)>* callback) override;
  std::unique_ptr<Delegate> Clone() override;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MANAGER_DELEGATE_H_
