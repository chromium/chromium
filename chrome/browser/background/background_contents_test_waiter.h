// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_TEST_WAITER_H_
#define CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_TEST_WAITER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_observer.h"

// A utility class to wait for a BackgroundContents to be created for a given
// app.
class BackgroundContentsTestWaiter : public BackgroundContentsServiceObserver {
 public:
  explicit BackgroundContentsTestWaiter(Profile* profile);
  ~BackgroundContentsTestWaiter() override;

  // Waits for a background contents for the given `application_id`. If a
  // background contents already exists and has loaded, returns immediately.
  void WaitForBackgroundContents(const std::string& application_id);

 private:
  // BackgroundContentsServiceObserver:
  void OnBackgroundContentsOpened(
      const BackgroundContentsOpenedDetails& details) override;

  base::ScopedObservation<BackgroundContentsService,
                          BackgroundContentsServiceObserver>
      scoped_observation_{this};
  raw_ptr<BackgroundContentsService> background_contents_service_;
  std::string application_id_;
  base::RunLoop run_loop_;
};

#endif  // CHROME_BROWSER_BACKGROUND_BACKGROUND_CONTENTS_TEST_WAITER_H_
