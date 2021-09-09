// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_PLAY_EXTRAS_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_PLAY_EXTRAS_H_

#include "chrome/browser/apps/app_discovery_service/result.h"

namespace apps {

class PlayExtras : public SourceExtras {
 public:
  explicit PlayExtras(const bool previously_installed);
  PlayExtras(const PlayExtras&) = delete;
  PlayExtras& operator=(const PlayExtras&) = delete;
  ~PlayExtras() override = default;

  // Whether or not this app was previously installed on a different device
  // that this user owns.
  bool GetPreviouslyInstalled() const;

  // Result::SourceExtras:
  PlayExtras* AsPlayExtras() override;

 private:
  const bool previously_installed_;
  // TODO(crbug.com/1223321): Add remaining fields.
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_PLAY_EXTRAS_H_
