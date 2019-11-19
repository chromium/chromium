// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_SOURCE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_SOURCE_H_

#include "base/macros.h"
#include "content/public/browser/url_data_source.h"

class Profile;

namespace apps {

// AppIconSource serves app icons through the AppServiceProxy.
// Icons can be retrieved for any installed app.
//
// To request an icon the AppIconSource must have been initialized via
// content::URLDataSource::Add().
//
// The format for requesting an icon is as follows:
//   chrome://app-icon/<app_id>/<icon_size>
//
//   Parameters:
//    <app_id>    = the id of the app
//    <icon_size> = the desired size in DIP of the icon
//
// We attempt to load icons from the following sources in order:
//  1) The icon listed through the AppServiceProxy
//  2) The default icon if there are no matches

class AppIconSource : public content::URLDataSource {
 public:
  explicit AppIconSource(Profile* profile);
  ~AppIconSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string&) override;
  bool AllowCaching() override;
  bool ShouldReplaceExistingSource() override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AppIconSource);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_SOURCE_H_
