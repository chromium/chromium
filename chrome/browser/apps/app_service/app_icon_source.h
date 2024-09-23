// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_SOURCE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_SOURCE_H_

#include <string>

#include "base/memory/raw_ptr.h"
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
  AppIconSource(const AppIconSource&) = delete;
  AppIconSource& operator=(const AppIconSource&) = delete;
  ~AppIconSource() override;

  static GURL GetIconURL(const std::string& app_id, int icon_size);

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL&) override;
  bool AllowCaching() override;
  bool ShouldReplaceExistingSource() override;

 private:
  // TODO(crbug.com/40261613): Prevent this pointer from dangling.
  const raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_SOURCE_H_
