// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/face_ml/chrome_face_ml_user_provider.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {
mojom::face_ml_app::UserInformation ExampleUserInformation() {
  return mojom::face_ml_app::UserInformation(/*user_name=*/"user@gmail.com",
                                             /*is_signed_in=*/false);
}
}  // namespace

class ChromeFaceMLUserProviderTest : public BrowserWithTestWindowTest {
 public:
  ChromeFaceMLUserProviderTest()
      : user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_(
            std::unique_ptr<FakeChromeUserManager>(user_manager_)) {}

  std::unique_ptr<TestingProfile> CreateRegularProfile() {
    AccountId account_id_ =
        AccountId::FromUserEmailGaiaId("user@gmail.com", "12345");
    user_manager_->AddUser(account_id_);
    TestingProfile::Builder builder;
    builder.SetProfileName("user@gmail.com");
    std::unique_ptr<TestingProfile> profile = builder.Build();
    return profile;
  }

 protected:
  raw_ptr<FakeChromeUserManager, ExperimentalAsh> user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

// Tests ChromeFaceMLUserProvider provides correct user profile from browser
// context to FaceML.
TEST_F(ChromeFaceMLUserProviderTest,
       GetCurrentUserInformationReturnsCurrentProfileFromeBrowser) {
  std::unique_ptr<TestingProfile> profile = CreateRegularProfile();
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile.get())));
  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents.get());

  ChromeFaceMLUserProvider chrome_user_provider{&test_web_ui};
  mojom::face_ml_app::UserInformation user_info =
      chrome_user_provider.GetCurrentUserInformation();

  EXPECT_EQ(user_info, ExampleUserInformation());
}

}  // namespace ash
