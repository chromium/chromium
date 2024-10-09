// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list_mock_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::WebContents;
using content::WebContentsTester;

namespace {

constexpr int kDefaultSourceCount = 2;
constexpr int kThumbnailSize = 50;

// Create a greyscale image with certain size and grayscale value.
gfx::Image CreateGrayscaleImage(gfx::Size size, uint8_t greyscale_value) {
  SkBitmap result;
  result.allocN32Pixels(size.width(), size.height(), true);

  uint8_t* pixels_data = reinterpret_cast<uint8_t*>(result.getPixels());

  // Set greyscale value for all pixels.
  for (int y = 0; y < result.height(); ++y) {
    for (int x = 0; x < result.width(); ++x) {
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel()] =
          greyscale_value;
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 1] =
          greyscale_value;
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 2] =
          greyscale_value;
      pixels_data[result.rowBytes() * y + x * result.bytesPerPixel() + 3] =
          0xff;
    }
  }

  return gfx::Image::CreateFrom1xBitmap(result);
}

ACTION_P2(CheckListSize, list, expected_list_size) {
  EXPECT_EQ(expected_list_size, list->GetSourceCount());
}

// This is a helper class to abstract away some of the details of creating and
// managing the life-cycle of an AppWindow
class TestAppWindow : public content::WebContentsObserver {
 public:
  TestAppWindow(const TestAppWindow&) = delete;
  TestAppWindow& operator=(const TestAppWindow&) = delete;

  TestAppWindow(Profile* profile,
                const extensions::Extension* extension,
                std::unique_ptr<content::WebContents> contents) {
    window_ = new extensions::AppWindow(
        profile,
        std::make_unique<ChromeAppDelegate>(profile, /*keep_alive=*/false),
        extension);
    window_->SetAppWindowContentsForTesting(
        std::make_unique<extensions::TestAppWindowContents>(
            std::move(contents)));

    extensions::AppWindowRegistry::Get(profile)->AddAppWindow(window_);
    Observe(window_->web_contents());
  }

  ~TestAppWindow() override { Close(); }

  void Close() {
    if (!window_)
      return;

    content::WebContentsDestroyedWatcher destroyed_watcher(
        window_->web_contents());
    window_->OnNativeClose();
    destroyed_watcher.Wait();

    EXPECT_FALSE(window_);
  }

  void WebContentsDestroyed() override { window_ = nullptr; }

 private:
  raw_ptr<extensions::AppWindow> window_;
};

}  // namespace

class TabDesktopMediaListTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 public:
  TabDesktopMediaListTest(const TabDesktopMediaListTest&) = delete;
  TabDesktopMediaListTest& operator=(const TabDesktopMediaListTest&) = delete;

 protected:
  TabDesktopMediaListTest()
      : picker_called_from_web_contents_(GetParam()),
        local_state_(TestingBrowserProcess::GetGlobal()) {}

  std::unique_ptr<content::WebContents> CreateWebContents(
      int favicon_greyscale) {
    std::unique_ptr<WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(
            profile_, content::SiteInstance::Create(profile_)));
    if (!contents)
      return nullptr;

    WebContentsTester::For(contents.get())
        ->SetLastActiveTimeTicks(base::TimeTicks::Now());

    // Get or create a NavigationEntry and add a title and a favicon to it.
    content::NavigationEntry* entry =
        contents->GetController().GetLastCommittedEntry();
    if (!entry) {
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          contents.get(), GURL("chrome://blank"));
      entry = contents->GetController().GetLastCommittedEntry();
    }

    contents->UpdateTitleForEntry(entry, u"Test tab");

    content::FaviconStatus favicon_info;
    favicon_info.image =
        CreateGrayscaleImage(gfx::Size(10, 10), favicon_greyscale);
    entry->GetFavicon() = favicon_info;

    return contents;
  }

  void AddWebcontents(int favicon_greyscale) {
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    ASSERT_TRUE(tab_strip_model);
    auto contents = CreateWebContents(favicon_greyscale);
    ASSERT_TRUE(contents);
    manually_added_web_contents_.push_back(contents.get());
    tab_strip_model->AppendWebContents(std::move(contents), true);
  }

  const extensions::Extension* BuildOrGetExtension() {
    if (!extension_) {
      extension_ =
          extensions::ExtensionBuilder()
              .SetManifest(base::Value::Dict()
                               .Set("name", "TabListUnitTest Extension")
                               .Set("version", "1.0")
                               .Set("manifest_version", 2))
              .Build();
    }
    return extension_.get();
  }

  void AddAppWindow() {
    auto app_window = std::make_unique<TestAppWindow>(
        profile_, BuildOrGetExtension(), CreateWebContents(10));

    manually_added_app_windows_.push_back(std::move(app_window));
  }

  void SetUp() override {
    rvh_test_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    // Create a new temporary directory, and store the path.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::make_unique<FakeProfileManager>(temp_dir_.GetPath()));

    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    cl->AppendSwitch(switches::kNoFirstRun);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    cl->AppendSwitch(switches::kTestType);
#endif

    // Create profile.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    ASSERT_TRUE(profile_manager);

    profile_ = profile_manager->GetLastUsedProfileAllowedByPolicy();
    ASSERT_TRUE(profile_);

    // Create browser.
    Browser::CreateParams profile_params(profile_, true);
    browser_ = CreateBrowserWithTestWindowForParams(profile_params);
    ASSERT_TRUE(browser_);
    for (int i = 0; i < kDefaultSourceCount; i++) {
      AddWebcontents(i + 1);
    }
  }

  void TearDown() override {
    list_.reset();

    // TODO(erikchen): Tearing down the TabStripModel should just delete all its
    // owned WebContents. Then |manually_added_web_contents_| won't be
    // necessary. https://crbug.com/832879.
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    for (WebContents* contents : manually_added_web_contents_) {
      tab_strip_model->DetachAndDeleteWebContentsAt(
          tab_strip_model->GetIndexOfWebContents(contents));
    }
    manually_added_web_contents_.clear();
    manually_added_app_windows_.clear();

    browser_.reset();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    base::RunLoop().RunUntilIdle();
    rvh_test_enabler_.reset();
  }

  void CreateDefaultList() {
    DCHECK(!list_);

    content::WebContents* const web_contents =
        picker_called_from_web_contents_
            ? browser_->tab_strip_model()->GetWebContentsAt(0)
            : nullptr;

    // The actual "default" for |include_chrome_app_windows| is false; but for
    // the purposes of the tests we make the default true, so that all paths are
    // exercised.
    list_ = std::make_unique<TabDesktopMediaList>(
        web_contents, base::BindRepeating([](content::WebContents* contents) {
          return true;
        }),
        /*include_chrome_app_windows=*/true);
    list_->SetThumbnailSize(gfx::Size(kThumbnailSize, kThumbnailSize));

    // Set update period to reduce the time it takes to run tests.
    // >0 to avoid unit test failure.
    list_->SetUpdatePeriod(base::Milliseconds(1));
  }

  void InitializeAndVerify() {
    CreateDefaultList();
    base::RunLoop loop;
    // The tabs in media source list are sorted in decreasing time order. The
    // latest one is listed first. However, tabs are added to TabStripModel in
    // increasing time order, the oldest one is added first.
    {
      testing::InSequence dummy;

      for (int i = 0; i < kDefaultSourceCount; i++) {
        EXPECT_CALL(observer_, OnSourceAdded(i))
            .WillOnce(CheckListSize(list_.get(), i + 1));
      }

      for (int i = 0; i < kDefaultSourceCount - 1; i++) {
        EXPECT_CALL(observer_,
                    OnSourceThumbnailChanged(kDefaultSourceCount - 1 - i));
      }
      EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
          .WillOnce(base::test::RunClosure(loop.QuitClosure()));
    }

    list_->StartUpdating(&observer_);
    loop.Run();

    for (int i = 0; i < kDefaultSourceCount; ++i) {
      EXPECT_EQ(list_->GetSource(i).id.type,
                content::DesktopMediaID::TYPE_WEB_CONTENTS);
    }

    observer_.VerifyAndClearExpectations();
  }

  const bool picker_called_from_web_contents_;

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;

  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<Browser> browser_;

  // Must be listed before |list_|, so it's destroyed last.
  DesktopMediaListMockObserver observer_;
  std::unique_ptr<TabDesktopMediaList> list_;
  std::vector<raw_ptr<WebContents, VectorExperimental>>
      manually_added_web_contents_;
  std::vector<std::unique_ptr<TestAppWindow>> manually_added_app_windows_;

  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<const extensions::Extension> extension_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          local_state_.Get(),
          ash::CrosSettings::Get())};
#endif
};

INSTANTIATE_TEST_SUITE_P(, TabDesktopMediaListTest, testing::Bool());

TEST_P(TabDesktopMediaListTest, AddTab) {
  InitializeAndVerify();
  base::RunLoop loop;
  AddWebcontents(10);

  EXPECT_CALL(observer_, OnSourceAdded(0))
      .WillOnce(CheckListSize(list_.get(), kDefaultSourceCount + 1));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  loop.Run();
}

TEST_P(TabDesktopMediaListTest, AddAppWindow) {
  InitializeAndVerify();

  AddAppWindow();

  base::RunLoop loop;
  // Note that unlike adding a tab, our AppWindow that we add is only
  // initialized enough to show up in the list; it doesn't have a favicon driver
  // which would be needed to extract the favicon from it.
  EXPECT_CALL(observer_, OnSourceAdded(0))
      .WillOnce(
          testing::DoAll(CheckListSize(list_.get(), kDefaultSourceCount + 1),
                         base::test::RunClosure(loop.QuitClosure())));

  loop.Run();
}

TEST_P(TabDesktopMediaListTest, RemoveTab) {
  InitializeAndVerify();
  base::RunLoop loop;
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  std::unique_ptr<tabs::TabModel> detached_tab =
      tab_strip_model->DetachTabAtForInsertion(kDefaultSourceCount - 1);
  std::erase(manually_added_web_contents_, detached_tab.get()->contents());

  EXPECT_CALL(observer_, OnSourceRemoved(0))
      .WillOnce(
          testing::DoAll(CheckListSize(list_.get(), kDefaultSourceCount - 1),
                         base::test::RunClosure(loop.QuitClosure())));

  loop.Run();
}

TEST_P(TabDesktopMediaListTest, MoveTab) {
  InitializeAndVerify();
  base::RunLoop loop;
  // Swap the two media sources by swap their time stamps.
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  WebContents* contents0 = tab_strip_model->GetWebContentsAt(0);
  ASSERT_TRUE(contents0);
  base::TimeTicks t0 = contents0->GetLastActiveTimeTicks();
  WebContents* contents1 = tab_strip_model->GetWebContentsAt(1);
  ASSERT_TRUE(contents1);
  base::TimeTicks t1 = contents1->GetLastActiveTimeTicks();

  WebContentsTester::For(contents0)->SetLastActiveTimeTicks(t1);
  WebContentsTester::For(contents1)->SetLastActiveTimeTicks(t0);

  EXPECT_CALL(observer_, OnSourceMoved(1, 0))
      .WillOnce(testing::DoAll(CheckListSize(list_.get(), kDefaultSourceCount),
                               base::test::RunClosure(loop.QuitClosure())));

  loop.Run();
}

TEST_P(TabDesktopMediaListTest, UpdateTitle) {
  InitializeAndVerify();
  base::RunLoop loop;
  // Change tab's title.
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  WebContents* contents =
      tab_strip_model->GetWebContentsAt(kDefaultSourceCount - 1);
  ASSERT_TRUE(contents);
  content::NavigationController& controller = contents->GetController();
  contents->UpdateTitleForEntry(controller.GetLastCommittedEntry(),
                                u"New test tab");

  EXPECT_CALL(observer_, OnSourceNameChanged(0))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  loop.Run();

  EXPECT_EQ(list_->GetSource(0).name, u"New test tab");
}

TEST_P(TabDesktopMediaListTest, UpdateThumbnail) {
  InitializeAndVerify();

  base::RunLoop loop;
  // Change tab's favicon.
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  WebContents* contents =
      tab_strip_model->GetWebContentsAt(kDefaultSourceCount - 1);
  ASSERT_TRUE(contents);

  content::FaviconStatus favicon_info;
  favicon_info.image = CreateGrayscaleImage(gfx::Size(10, 10), 100);
  contents->GetController().GetLastCommittedEntry()->GetFavicon() =
      favicon_info;

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  loop.Run();
}

// Test that a source which is set as the one being previewed is marked as being
// visibly captured, so that it is still painted even when hidden.
TEST_P(TabDesktopMediaListTest, SetPreviewMarksTabAsVisiblyCaptured) {
  InitializeAndVerify();
  base::RunLoop loop;
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  WebContents* contents =
      tab_strip_model->GetWebContentsAt(kDefaultSourceCount - 1);
  ASSERT_TRUE(contents);

  list_->SetPreviewedSource(list_->GetSource(0).id);

  EXPECT_TRUE(contents->IsBeingVisiblyCaptured());

  EXPECT_CALL(observer_, OnSourcePreviewChanged(0))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  loop.Run();
}
