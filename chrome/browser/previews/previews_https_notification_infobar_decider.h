// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_HTTPS_NOTIFICATION_INFOBAR_DECIDER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_HTTPS_NOTIFICATION_INFOBAR_DECIDER_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

class PrefService;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

// This specifies an interface for deciding to and displaying the InfoBar that
// tells the user that Data Saver now also optimizes HTTPS pages.
class PreviewsHTTPSNotificationInfoBarDecider
    : public data_reduction_proxy::DataReductionProxySettingsObserver {
 public:
  explicit PreviewsHTTPSNotificationInfoBarDecider(
      content::BrowserContext* browser_context);
  virtual ~PreviewsHTTPSNotificationInfoBarDecider();

  // Registers the prefs used in this class.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Removes |this| as a DataReductionProxySettingsObserver.
  void Shutdown();

  // Note: |NeedsToToNotify| is intentionally separate from |NotifyUser| for
  // ease of testing and metrics collection without changing the notification
  // state.
  // Returns true if the UI notification needs to be shown to the user before
  // this preview can be shown.
  bool NeedsToNotifyUser();

  // Prompts |this| to display the required UI notifications to the user.
  void NotifyUser(content::WebContents* web_contents);

  // Sets that the user has seen the UI notification.
  void SetUserHasSeenUINotification();

  // data_reduction_proxy::DataReductionProxySettingsObserver:
  void OnSettingsInitialized() override;

 private:
  // A reference to the DRP Settings so that |this| can be removed as an
  // observer on |Shutdown|. Not owned.
  data_reduction_proxy::DataReductionProxySettings* drp_settings_ = nullptr;

  // A reference to the profile's |PrefService|.
  PrefService* pref_service_ = nullptr;

  // Whether the notification infobar needs to be shown to the user.
  bool need_to_show_notification_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsHTTPSNotificationInfoBarDecider);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_HTTPS_NOTIFICATION_INFOBAR_DECIDER_H_
