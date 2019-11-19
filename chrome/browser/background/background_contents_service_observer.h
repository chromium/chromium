// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

struct BackgroundContentsOpenedDetails;

class BackgroundContentsServiceObserver : public base::CheckedObserver {
 public:
  virtual void OnBackgroundContentsServiceChanged() {}
  virtual void OnBackgroundContentsOpened(
      const BackgroundContentsOpenedDetails& details) {}
  virtual void OnBackgroundContentsServiceDestroying() {}

 protected:
  ~BackgroundContentsServiceObserver() override = default;
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_SERVICE_OBSERVER_H_
