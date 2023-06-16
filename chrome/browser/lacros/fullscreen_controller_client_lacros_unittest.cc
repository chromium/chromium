// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/fullscreen_controller_client_lacros.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chromeos/crosapi/mojom/fullscreen_controller.mojom.h"
#include "chromeos/ui/wm/fullscreen/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/common/extension_builder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

namespace chromeos {

using crosapi::mojom::FullscreenController;
using crosapi::mojom::FullscreenControllerClient;

namespace {

constexpr char kActiveUrl[] = "https://wwww.test.com";

constexpr char kNonMatchingPattern[] = "google.com";
constexpr char kMatchingPattern[] = "test.com";
constexpr char kWildcardPattern[] = "*";

enum class TestWebContentsChoice {
  kAppWindow,
  kBrowserWindow,
};

std::string ParamToString(
    const testing::TestParamInfo<TestWebContentsChoice>& info) {
  switch (info.param) {
    case TestWebContentsChoice::kAppWindow:
      return "AppWindow";
    case TestWebContentsChoice::kBrowserWindow:
      return "BrowserWindow";
  }
}

class MockRemote : public FullscreenController {
 public:
  // FullscreenController:
  void AddClient(
      mojo::PendingRemote<FullscreenControllerClient> client) override {
    remote_.Bind(std::move(client));
  }

  void ShouldExitFullscreenBeforeLock(base::OnceCallback<void(bool)> callback) {
    remote_->ShouldExitFullscreenBeforeLock(
        base::BindOnce(&MockRemote::OnShouldExitFullscreenBeforeLock,
                       base::Unretained(this), std::move(callback)));
  }

  void OnShouldExitFullscreenBeforeLock(base::OnceCallback<void(bool)> callback,
                                        bool should_exit_fullscreen) {
    std::move(callback).Run(should_exit_fullscreen);
  }

 private:
  mojo::Receiver<FullscreenController> receiver_{this};
  mojo::Remote<FullscreenControllerClient> remote_;
};

class TestNativeAppWindow : public ChromeNativeAppWindowViewsAura {
 public:
  TestNativeAppWindow() {
    set_web_view_for_testing(
        AddChildView(std::make_unique<views::WebView>(nullptr)));
  }
  ~TestNativeAppWindow() override {}

  bool IsActive() const override { return true; }
};

}  // namespace

class FullscreenControllerClientLacrosTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    profile_ = ProfileManager::GetPrimaryUserProfile();

    // Set the active profile.
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetString(::prefs::kProfileLastUsed,
                           profile_->GetBaseName().value());
  }

  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }

  void SetKeepFullscreenWithoutNotificationAllowList(
      const std::string& pattern) {
    base::Value::List list;
    list.Append(pattern);
    profile_->GetPrefs()->SetList(
        prefs::kKeepFullscreenWithoutNotificationUrlAllowList, std::move(list));
  }

  void RunTest(bool expect_should_exit_fullscreen) {
    base::test::TestFuture<bool> future;

    FullscreenControllerClientLacros client;
    mojo::Receiver<FullscreenControllerClient> receiver{&client};
    mock_.AddClient(receiver.BindNewPipeAndPassRemote());
    mock_.ShouldExitFullscreenBeforeLock(future.GetCallback());

    bool should_exit_fullscreen = future.Take();
    ASSERT_EQ(should_exit_fullscreen, expect_should_exit_fullscreen);
  }

 protected:
  raw_ptr<Profile> profile_ = nullptr;
  testing::StrictMock<MockRemote> mock_;
};

// Test that ShouldExitFullscreenBeforeLock() returns true if the WebContent is
// not found and the allow list is unset.
TEST_F(FullscreenControllerClientLacrosTest,
       ExitFullscreenIfWebContentsUnavailableUnsetPref) {
  RunTest(/*expect_should_exit_fullscreen=*/true);
}

// Test that ShouldExitFullscreenBeforeLock() returns true if the WebContent is
// not found and the allow list includes the wildcard character.
TEST_F(FullscreenControllerClientLacrosTest,
       ExitFullscreenIfWebContentsUnavailableWildcardPref) {
  SetKeepFullscreenWithoutNotificationAllowList(kWildcardPattern);

  RunTest(/*expect_should_exit_fullscreen=*/true);
}

class FullscreenControllerClientLacrosWebContentsTest
    : public FullscreenControllerClientLacrosTest,
      public testing::WithParamInterface<TestWebContentsChoice> {
 public:
  void SetUp() override {
    FullscreenControllerClientLacrosTest::SetUp();

    switch (GetParam()) {
      case TestWebContentsChoice::kAppWindow:
        AddAppWindow();
        break;
      case TestWebContentsChoice::kBrowserWindow:
        AddBrowserWindow();
        break;
    }
  }

  void TearDown() override {
    if (app_window_) {
      app_window_->OnNativeClose();
      app_window_ = nullptr;
    }

    FullscreenControllerClientLacrosTest::TearDown();
  }

  void AddAppWindow() {
    // Create a new AppWindow
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("test extension").Build();
    app_window_ = new extensions::AppWindow(
        profile_, std::make_unique<ChromeAppDelegate>(profile_, true),
        extension.get());

    // Set the active WebContents
    std::unique_ptr<content::WebContents> contents(content::WebContents::Create(
        content::WebContents::CreateParams(app_window_->browser_context())));
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        contents.get(), GURL(kActiveUrl));
    app_window_->SetAppWindowContentsForTesting(
        std::make_unique<extensions::TestAppWindowContents>(
            std::move(contents)));

    // Set the native app window
    app_window_->SetNativeAppWindowForTesting(
        std::make_unique<TestNativeAppWindow>());

    extensions::AppWindowRegistry::Get(profile_)->AddAppWindow(app_window_);
  }

  void AddBrowserWindow() {
    AddTab(browser(), GURL(kActiveUrl));
    static_cast<TestBrowserWindow*>(browser()->window())->set_is_active(true);
    ASSERT_TRUE(chrome::FindBrowserWithActiveWindow());
  }

 protected:
  raw_ptr<extensions::AppWindow> app_window_ = nullptr;
};

// Test that ShouldExitFullscreenBeforeLock() returns true if the allow list
// pref is unset.
TEST_P(FullscreenControllerClientLacrosWebContentsTest,
       ExitFullscreenIfUnsetPref) {
  RunTest(/*expect_should_exit_fullscreen=*/true);
}

// Test that ShouldExitFullscreenBeforeLock() returns true if the URL of the
// active window does not match any patterns from the allow list.
TEST_P(FullscreenControllerClientLacrosWebContentsTest,
       ExitFullscreenIfNonMatchingPref) {
  SetKeepFullscreenWithoutNotificationAllowList(kNonMatchingPattern);

  RunTest(/*expect_should_exit_fullscreen=*/true);
}

// Test that ShouldExitFullscreenBeforeLock() returns false if the URL of the
// active window matches a pattern from the allow list.
TEST_P(FullscreenControllerClientLacrosWebContentsTest,
       KeepFullscreenIfMatchingPref) {
  // Set up the URL exempt list with one matching and one non-matching pattern.
  base::Value::List list;
  list.Append(kNonMatchingPattern);
  list.Append(kMatchingPattern);
  profile_->GetPrefs()->SetList(
      prefs::kKeepFullscreenWithoutNotificationUrlAllowList, std::move(list));

  RunTest(/*expect_should_exit_fullscreen=*/false);
}

// Test that ShouldExitFullscreenBeforeLock() returns false if the allow list
// includes the wildcard character.
TEST_P(FullscreenControllerClientLacrosWebContentsTest,
       KeepFullscreenIfWildcardPref) {
  SetKeepFullscreenWithoutNotificationAllowList(kWildcardPattern);

  RunTest(/*expect_should_exit_fullscreen=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FullscreenControllerClientLacrosWebContentsTest,
    ::testing::Values(TestWebContentsChoice::kAppWindow,
                      TestWebContentsChoice::kBrowserWindow),
    &ParamToString);

}  // namespace chromeos
