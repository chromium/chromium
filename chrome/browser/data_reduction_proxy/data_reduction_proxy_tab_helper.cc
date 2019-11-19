// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_tab_helper.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

DataReductionProxyTabHelper::DataReductionProxyTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  drp_settings_ = DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  if (drp_settings_)
    drp_settings_->AddDataReductionProxySettingsObserver(this);
}

DataReductionProxyTabHelper::~DataReductionProxyTabHelper() {
  if (drp_settings_)
    drp_settings_->RemoveDataReductionProxySettingsObserver(this);
}

void DataReductionProxyTabHelper::OnDataSaverEnabledChanged(bool enabled) {
  // This is a fairly expensive call, so it is done on a task thread. This
  // prevents the UI thread being blocked on sending messages to every running
  // renderer.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyTabHelper::UpdateWebkitPreferencesNow,
                     weak_factory_.GetWeakPtr()));
}

void DataReductionProxyTabHelper::UpdateWebkitPreferencesNow() {
  web_contents()->GetRenderViewHost()->OnWebkitPreferencesChanged();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DataReductionProxyTabHelper)
