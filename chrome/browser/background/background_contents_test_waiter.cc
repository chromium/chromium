// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_contents_test_waiter.h"

#include "chrome/browser/background/background_contents_service_factory.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

BackgroundContentsTestWaiter::BackgroundContentsTestWaiter(Profile* profile)
    : background_contents_service_(
          BackgroundContentsServiceFactory::GetForProfile(profile)) {}
BackgroundContentsTestWaiter::~BackgroundContentsTestWaiter() = default;

void BackgroundContentsTestWaiter::WaitForBackgroundContents(
    const std::string& application_id) {
  BackgroundContents* background_contents =
      background_contents_service_->GetAppBackgroundContents(application_id);
  if (!background_contents) {
    application_id_ = application_id;
    scoped_observation_.Observe(background_contents_service_);
    run_loop_.Run();
    background_contents =
        background_contents_service_->GetAppBackgroundContents(application_id);
  }

  ASSERT_TRUE(background_contents);
  // On windows, the background contents sometimes isn't seen as loading
  // successfully for some unknown reason. This doesn't impact these tests,
  // which only rely on the creation of the web contents.
  content::WaitForLoadStopWithoutSuccessCheck(
      background_contents->web_contents());
}

void BackgroundContentsTestWaiter::OnBackgroundContentsOpened(
    const BackgroundContentsOpenedDetails& details) {
  if (details.application_id == application_id_) {
    run_loop_.QuitWhenIdle();
  }
}
