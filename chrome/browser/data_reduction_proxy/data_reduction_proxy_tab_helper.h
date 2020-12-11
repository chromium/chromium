// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_TAB_HELPER_H_
#define CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_TAB_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Tracks when the Data Saver setting is enabled or disabled and notifies
// content.
class DataReductionProxyTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DataReductionProxyTabHelper>,
      public data_reduction_proxy::DataReductionProxySettingsObserver {
 public:
  ~DataReductionProxyTabHelper() override;

  // data_reduction_proxy::DataReductionProxySettingsObserver:
  void OnDataSaverEnabledChanged(bool enabled) override;

 private:
  friend class content::WebContentsUserData<DataReductionProxyTabHelper>;

  explicit DataReductionProxyTabHelper(content::WebContents* web_contents);

  // Notifies the RenderViewHost to update Blink's preferences. This is a fairly
  // expensive call, so only run in a task thread. Actually setting whether Data
  // Saver is enabled in the Blink prefs message is done in
  // |ChromeContentBrowserClient::OverrideWebkitPrefs|. This method just kicks
  // off that code path.
  void UpdateWebkitPreferencesNow();

  data_reduction_proxy::DataReductionProxySettings* drp_settings_;

  base::WeakPtrFactory<DataReductionProxyTabHelper> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyTabHelper);
};

#endif  // CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_TAB_HELPER_H_
