// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

class GURL;
class HostContentSettingsMap;
class Profile;

namespace base {
class DictionaryValue;
}  // namespace base

// This class contains helpers to get/set content and website settings related
// to subresource filtering.
// TODO(crbug.com/706061): Once observing changes to content settings is robust
// enough for metrics collection, should collect metrics here too, using a
// content_settings::Observer. Generally speaking, we want a system where we can
// easily log metrics if the content setting has changed meaningfully from it's
// previous value.
class SubresourceFilterContentSettingsManager
    : public history::HistoryServiceObserver {
 public:
  explicit SubresourceFilterContentSettingsManager(Profile* profile);
  ~SubresourceFilterContentSettingsManager() override;

  ContentSetting GetSitePermission(const GURL& url) const;

  // Only called via direct user action on via the subresource filter UI. Sets
  // the content setting to turn off the subresource filter.
  void WhitelistSite(const GURL& url);

  // Public for testing.
  std::unique_ptr<base::DictionaryValue> GetSiteMetadata(const GURL& url) const;

  // Specific logic for more intelligent UI.
  void OnDidShowUI(const GURL& url);
  bool ShouldShowUIForSite(const GURL& url) const;
  bool should_use_smart_ui() const { return should_use_smart_ui_; }
  void set_should_use_smart_ui_for_testing(bool should_use_smart_ui) {
    should_use_smart_ui_ = should_use_smart_ui;
  }

  // If the site is no longer activated, clear the metadata.
  //
  // If the site is activated, ensure that there is metadata. Don't log a
  // timestamp since the timestamp implies that the UI has been shown.
  //
  // This is to maintain the invariant that metadata implies activated.
  void ResetSiteMetadataBasedOnActivation(const GURL& url, bool is_activated);

  void set_clock_for_testing(std::unique_ptr<base::Clock> tick_clock) {
    clock_ = std::move(tick_clock);
  }

  // Time before showing the UI again on a domain.
  // TODO(csharrison): Consider setting this via a finch param.
  static constexpr base::TimeDelta kDelayBeforeShowingInfobarAgain =
      base::TimeDelta::FromHours(24);

 private:
  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  void SetSiteMetadata(const GURL& url,
                       std::unique_ptr<base::DictionaryValue> dict);

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_observer_{this};

  HostContentSettingsMap* settings_map_;

  // A clock is injected into this class so tests can set arbitrary timestamps
  // in website settings.
  std::unique_ptr<base::Clock> clock_;

  bool should_use_smart_ui_ = false;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterContentSettingsManager);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_CONTENT_SETTINGS_MANAGER_H_
