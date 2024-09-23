// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_bindings.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class DevToolsUIBindingsTest : public testing::Test {
};

TEST_F(DevToolsUIBindingsTest, SanitizeFrontendURL) {
  std::vector<std::pair<std::string, std::string>> tests = {
      {"random-string", "devtools://devtools/"},
      {"http://valid.url/but/wrong", "devtools://devtools/but/wrong"},
      {"devtools://wrong-domain/", "devtools://devtools/"},
      {"devtools://devtools/bundled/devtools.html",
       "devtools://devtools/bundled/devtools.html"},
      {"devtools://devtools:1234/bundled/devtools.html#hash",
       "devtools://devtools/bundled/devtools.html#hash"},
      {"devtools://devtools/some/random/path",
       "devtools://devtools/some/random/path"},
      {"devtools://devtools/bundled/devtools.html?debugFrontend=true",
       "devtools://devtools/bundled/devtools.html?debugFrontend=true"},
      {"devtools://devtools/bundled/devtools.html"
       "?some-flag=flag&v8only=true&debugFrontend=a"
       "&another-flag=another-flag&can_dock=false&isSharedWorker=notreally"
       "&remoteFrontend=sure",
       "devtools://devtools/bundled/devtools.html"
       "?v8only=true&debugFrontend=true"
       "&can_dock=true&isSharedWorker=true&remoteFrontend=true"},
      {"devtools://devtools/?ws=any-value-is-fine",
       "devtools://devtools/?ws=any-value-is-fine"},
      {"devtools://devtools/"
       "?service-backend=ws://localhost:9222/services",
       "devtools://devtools/"
       "?service-backend=ws://localhost:9222/services"},
      {"devtools://devtools/?remoteBase="
       "http://example.com:1234/remote-base#hash",
       "devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/"
       "serve_file//#hash"},
      {"devtools://devtools/?ws=1%26evil%3dtrue",
       "devtools://devtools/?ws=1%26evil%3dtrue"},
      {"devtools://devtools/?ws=encoded-ok'",
       "devtools://devtools/?ws=encoded-ok%27"},
      {"devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/some/path/"
       "@123719741873/more/path.html",
       "devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/serve_file/path/"},
      {"devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@123719741873/inspector.html%3FdebugFrontend%3Dfalse",
       "devtools://devtools/?remoteBase="
       "https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@123719741873/"},
      {"devtools://devtools/bundled/inspector.html?"
       "&remoteBase=https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@b4907cc5d602ff470740b2eb6344b517edecb7b9/&can_dock=true",
       "devtools://devtools/bundled/inspector.html?"
       "remoteBase=https://chrome-devtools-frontend.appspot.com/serve_file/"
       "@b4907cc5d602ff470740b2eb6344b517edecb7b9/&can_dock=true"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html%3FdebugFrontend%3Dfalse",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Finspector.html%3FdebugFrontend%3Dtrue"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html%22></iframe>something",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Finspector.html"},
      {"devtools://devtools/?remoteFrontendUrl="
       "http://domain:1234/path/rev/a/filename.html%3Fparam%3Dvalue#hash",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2Frev%2Finspector.html#hash"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/devtools.html%3Fws%3Danyvalue"
       "&unencoded=value&debugFrontend=true",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Fdevtools.html%3Fws%3Danyvalue"
       "&debugFrontend=true"},
      {"devtools://devtools/?remoteFrontendUrl="
       "https://chrome-devtools-frontend.appspot.com/serve_rev/"
       "@12345/inspector.html%23%27",
       "devtools://devtools/?remoteFrontendUrl="
       "https%3A%2F%2Fchrome-devtools-frontend.appspot.com%2Fserve_rev"
       "%2F%4012345%2Finspector.html"},
      {"devtools://devtools/"
       "?enabledExperiments=explosionsWhileTyping;newA11yTool",
       "devtools://devtools/"
       "?enabledExperiments=explosionsWhileTyping;newA11yTool"},
      {"devtools://devtools/?enabledExperiments=invalidExperiment$",
       "devtools://devtools/"},
  };

  for (const auto& pair : tests) {
    GURL url = GURL(pair.first);
    url = DevToolsUIBindings::SanitizeFrontendURL(url);
    EXPECT_EQ(pair.second, url.spec());
  }
}

class DevToolsUIBindingsSyncInfoTest : public testing::Test {
 public:
  void SetUp() override {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext*) {
          return static_cast<std::unique_ptr<KeyedService>>(
              std::make_unique<syncer::TestSyncService>());
        }));
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(&profile_));
  }

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  TestingProfile profile_;
  raw_ptr<syncer::TestSyncService> sync_service_;
};

TEST_F(DevToolsUIBindingsSyncInfoTest, SyncDisabled) {
  sync_service_->SetSignedOut();

  base::Value::Dict info =
      DevToolsUIBindings::GetSyncInformationForProfile(&profile_);

  EXPECT_FALSE(info.FindBool("isSyncActive").value());
}

TEST_F(DevToolsUIBindingsSyncInfoTest, PreferencesNotSynced) {
  sync_service_->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks});

  base::Value::Dict info =
      DevToolsUIBindings::GetSyncInformationForProfile(&profile_);

  EXPECT_THAT(info.FindBool("isSyncActive"), testing::Optional(true));
  EXPECT_THAT(info.FindBool("arePreferencesSynced"), testing::Optional(false));
}

TEST_F(DevToolsUIBindingsSyncInfoTest, ImageAlwaysProvided) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "sync@devtools.dev", signin::ConsentLevel::kSync);
  sync_service_->SetSignedIn(signin::ConsentLevel::kSync, account_info);

  EXPECT_TRUE(account_info.account_image.IsEmpty());

  base::Value::Dict info =
      DevToolsUIBindings::GetSyncInformationForProfile(&profile_);

  EXPECT_EQ(*info.FindString("accountEmail"), "sync@devtools.dev");
  EXPECT_NE(info.FindString("accountImage"), nullptr);
}
