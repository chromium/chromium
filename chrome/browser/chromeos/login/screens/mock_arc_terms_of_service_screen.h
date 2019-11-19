// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_ARC_TERMS_OF_SERVICE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_ARC_TERMS_OF_SERVICE_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockArcTermsOfServiceScreen : public ArcTermsOfServiceScreen {
 public:
  MockArcTermsOfServiceScreen(ArcTermsOfServiceScreenView* view,
                              const ScreenExitCallback& exit_callback);
  ~MockArcTermsOfServiceScreen() override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  void ExitScreen(Result result);
};

class MockArcTermsOfServiceScreenView : public ArcTermsOfServiceScreenView {
 public:
  MockArcTermsOfServiceScreenView();
  ~MockArcTermsOfServiceScreenView() override;

  void AddObserver(ArcTermsOfServiceScreenViewObserver* observer) override;
  void RemoveObserver(ArcTermsOfServiceScreenViewObserver* observer) override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(Bind, void(ArcTermsOfServiceScreen* screen));
  MOCK_METHOD1(MockAddObserver,
               void(ArcTermsOfServiceScreenViewObserver* observer));
  MOCK_METHOD1(MockRemoveObserver,
               void(ArcTermsOfServiceScreenViewObserver* observer));

 private:
  ArcTermsOfServiceScreenViewObserver* observer_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_ARC_TERMS_OF_SERVICE_SCREEN_H_
