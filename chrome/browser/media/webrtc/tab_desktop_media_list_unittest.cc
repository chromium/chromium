// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif  // defined(OS_CHROMEOS)

using content::WebContents;
using content::WebContentsTester;

namespace {

constexpr int kDefaultSourceCount = 2;
constexpr int kThumbnailSize = 50;

class UnittestProfileManager : public ::ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ::ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  std::unique_ptr<Profile> CreateProfileHelper(
      const base::FilePath& path) override {
    if (!base::PathExists(path) && !base::CreateDirectory(path))
      return nullptr;
    return std::make_unique<TestingProfile>(path);
  }
};

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

}  // namespace

class MockObserver : public DesktopMediaListObserver {
 public:
  MOCK_METHOD2(OnSourceAdded, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceRemoved, void(DesktopMediaList* list, int index));
  MOCK_METHOD3(OnSourceMoved,
               void(DesktopMediaList* list, int old_index, int new_index));
  MOCK_METHOD2(OnSourceNameChanged, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceThumbnailChanged,
               void(DesktopMediaList* list, int index));
  MOCK_METHOD1(OnAllSourcesFound, void(DesktopMediaList* list));

  void VerifyAndClearExpectations() {
    testing::Mock::VerifyAndClearExpectations(this);
  }
};

ACTION_P2(CheckListSize, list, expected_list_size) {
  EXPECT_EQ(expected_list_size, list->GetSourceCount());
}

ACTION(QuitMessageLoop) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
}

class TabDesktopMediaListTest : public testing::Test {
 protected:
  TabDesktopMediaListTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void AddWebcontents(int favicon_greyscale) {
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    ASSERT_TRUE(tab_strip_model);
    std::unique_ptr<WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(
            profile_, content::SiteInstance::Create(profile_)));
    ASSERT_TRUE(contents);

    WebContentsTester::For(contents.get())
        ->SetLastActiveTime(base::TimeTicks::Now());

    // Get or create the transient NavigationEntry and add a title and a
    // favicon to it.
    content::NavigationEntry* entry =
        contents->GetController().GetTransientEntry();
    if (!entry) {
      std::unique_ptr<content::NavigationEntry> entry_new =
          content::NavigationController::CreateNavigationEntry(
              GURL("chrome://blank"), content::Referrer(), base::nullopt,
              ui::PAGE_TRANSITION_LINK, false, std::string(), profile_,
              nullptr /* blob_url_loader_factory */);

      contents->GetController().SetTransientEntry(std::move(entry_new));
      entry = contents->GetController().GetTransientEntry();
    }

    contents->UpdateTitleForEntry(entry, base::ASCIIToUTF16("Test tab"));

    content::FaviconStatus favicon_info;
    favicon_info.image =
        CreateGrayscaleImage(gfx::Size(10, 10), favicon_greyscale);
    entry->GetFavicon() = favicon_info;

    manually_added_web_contents_.push_back(contents.get());
    tab_strip_model->AppendWebContents(std::move(contents), true);
  }

  void SetUp() override {
    manually_added_web_contents_.clear();
    rvh_test_enabler_.reset(new content::RenderViewHostTestEnabler());
    // Create a new temporary directory, and store the path.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        new UnittestProfileManager(temp_dir_.GetPath()));

#if defined(OS_CHROMEOS)
    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    cl->AppendSwitch(switches::kTestType);
#endif

    // Create profile.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    ASSERT_TRUE(profile_manager);

    profile_ = profile_manager->GetLastUsedProfileAllowedByPolicy();
    ASSERT_TRUE(profile_);

    // Create browser.
    Browser::CreateParams profile_params(profile_, true);
    browser_ = CreateBrowserWithTestWindowForParams(&profile_params);
    ASSERT_TRUE(browser_);
    for (int i = 0; i < kDefaultSourceCount; i++) {
      AddWebcontents(i + 1);
    }
  }

  void TearDown() override {
    // TODO(erikchen): Tearing down the TabStripModel should just delete all its
    // owned WebContents. Then |manually_added_web_contents_| won't be
    // necessary. https://crbug.com/832879.
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    for (WebContents* contents : manually_added_web_contents_) {
      tab_strip_model->DetachWebContentsAt(
          tab_strip_model->GetIndexOfWebContents(contents));
    }
    manually_added_web_contents_.clear();

    browser_.reset();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(NULL);
    base::RunLoop().RunUntilIdle();
    rvh_test_enabler_.reset();
  }

  void CreateDefaultList() {
    list_.reset(new TabDesktopMediaList());
    list_->SetThumbnailSize(gfx::Size(kThumbnailSize, kThumbnailSize));

    // Set update period to reduce the time it takes to run tests.
    // >0 to avoid unit test failure.
    list_->SetUpdatePeriod(base::TimeDelta::FromMilliseconds(1));
  }

  void InitializeAndVerify() {
    CreateDefaultList();

    // The tabs in media source list are sorted in decreasing time order. The
    // latest one is listed first. However, tabs are added to TabStripModel in
    // increasing time order, the oldest one is added first.
    {
      testing::InSequence dummy;

      for (int i = 0; i < kDefaultSourceCount; i++) {
        EXPECT_CALL(observer_, OnSourceAdded(list_.get(), i))
            .WillOnce(CheckListSize(list_.get(), i + 1));
      }

      for (int i = 0; i < kDefaultSourceCount - 1; i++) {
        EXPECT_CALL(observer_, OnSourceThumbnailChanged(
                                   list_.get(), kDefaultSourceCount - 1 - i));
      }
      EXPECT_CALL(observer_, OnSourceThumbnailChanged(list_.get(), 0))
          .WillOnce(QuitMessageLoop());
    }

    list_->StartUpdating(&observer_);
    base::RunLoop().Run();

    for (int i = 0; i < kDefaultSourceCount; ++i) {
      EXPECT_EQ(list_->GetSource(i).id.type,
                content::DesktopMediaID::TYPE_WEB_CONTENTS);
    }

    observer_.VerifyAndClearExpectations();
  }

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;

  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  Profile* profile_;
  std::unique_ptr<Browser> browser_;

  // Must be listed before |list_|, so it's destroyed last.
  MockObserver observer_;
  std::unique_ptr<TabDesktopMediaList> list_;
  std::vector<WebContents*> manually_added_web_contents_;

  content::BrowserTaskEnvironment task_environment_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TabDesktopMediaListTest);
};

TEST_F(TabDesktopMediaListTest, AddTab) {
  InitializeAndVerify();

  AddWebcontents(10);

  EXPECT_CALL(observer_, OnSourceAdded(list_.get(), 0))
      .WillOnce(CheckListSize(list_.get(), kDefaultSourceCount + 1));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(list_.get(), 0))
      .WillOnce(QuitMessageLoop());

  base::RunLoop().Run();

  list_.reset();
}

TEST_F(TabDesktopMediaListTest, RemoveTab) {
  InitializeAndVerify();

  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  std::unique_ptr<WebContents> released_web_contents =
      tab_strip_model->DetachWebContentsAt(kDefaultSourceCount - 1);
  for (auto it = manually_added_web_contents_.begin();
       it != manually_added_web_contents_.end(); ++it) {
    if (*it == released_web_contents.get()) {
      manually_added_web_contents_.erase(it);
      break;
    }
  }

  EXPECT_CALL(observer_, OnSourceRemoved(list_.get(), 0))
      .WillOnce(
          testing::DoAll(CheckListSize(list_.get(), kDefaultSourceCount - 1),
                         QuitMessageLoop()));

  base::RunLoop().Run();

  list_.reset();
}

TEST_F(TabDesktopMediaListTest, MoveTab) {
  InitializeAndVerify();

  // Swap the two media sources by swap their time stamps.
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  WebContents* contents0 = tab_strip_model->GetWebContentsAt(0);
  ASSERT_TRUE(contents0);
  base::TimeTicks t0 = contents0->GetLastActiveTime();
  WebContents* contents1 = tab_strip_model->GetWebContentsAt(1);
  ASSERT_TRUE(contents1);
  base::TimeTicks t1 = contents1->GetLastActiveTime();

  WebContentsTester::For(contents0)->SetLastActiveTime(t1);
  WebContentsTester::For(contents1)->SetLastActiveTime(t0);

  EXPECT_CALL(observer_, OnSourceMoved(list_.get(), 1, 0))
      .WillOnce(testing::DoAll(CheckListSize(list_.get(), kDefaultSourceCount),
                               QuitMessageLoop()));

  base::RunLoop().Run();

  list_.reset();
}

TEST_F(TabDesktopMediaListTest, UpdateTitle) {
  InitializeAndVerify();

  // Change tab's title.
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  WebContents* contents =
      tab_strip_model->GetWebContentsAt(kDefaultSourceCount - 1);
  ASSERT_TRUE(contents);
  content::NavigationController& controller = contents->GetController();
  contents->UpdateTitleForEntry(controller.GetTransientEntry(),
                                base::ASCIIToUTF16("New test tab"));

  EXPECT_CALL(observer_, OnSourceNameChanged(list_.get(), 0))
      .WillOnce(QuitMessageLoop());

  base::RunLoop().Run();

  EXPECT_EQ(list_->GetSource(0).name, base::UTF8ToUTF16("New test tab"));

  list_.reset();
}

TEST_F(TabDesktopMediaListTest, UpdateThumbnail) {
  InitializeAndVerify();

  // Change tab's favicon.
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  WebContents* contents =
      tab_strip_model->GetWebContentsAt(kDefaultSourceCount - 1);
  ASSERT_TRUE(contents);

  content::FaviconStatus favicon_info;
  favicon_info.image = CreateGrayscaleImage(gfx::Size(10, 10), 100);
  contents->GetController().GetTransientEntry()->GetFavicon() = favicon_info;

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(list_.get(), 0))
      .WillOnce(QuitMessageLoop());

  base::RunLoop().Run();

  list_.reset();
}
