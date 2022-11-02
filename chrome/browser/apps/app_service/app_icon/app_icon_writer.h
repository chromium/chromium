// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_WRITER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_WRITER_H_

namespace apps {

// AppIconWriter writes app icons to the icon image files in the local disk.
//
// TODO(crbug.com/1380608): Implement the icon writing function.
class AppIconWriter {
 public:
  AppIconWriter();
  AppIconWriter(const AppIconWriter&) = delete;
  AppIconWriter& operator=(const AppIconWriter&) = delete;
  ~AppIconWriter();
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_WRITER_H_
