// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_user_provider_impl.h"

#include <memory>
#include <optional>

#include "ash/capture_mode/fake_video_source_provider.h"
#include "ash/public/cpp/default_user_image.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/users/avatar/fake_user_image_file_selector.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/avatar/user_image_prefs.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace ash::personalization_app {

namespace {

using ash::personalization_app::GetAccountId;

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr char kFakeTestName[] = "Fake Name";
constexpr char kTestGaiaId[] = "1234567890";

mojo_base::BigBuffer FakeEncodedPngBuffer() {
  return mojo_base::BigBuffer({0, 1});
}

class TestUserImageObserver
    : public ash::personalization_app::mojom::UserImageObserver {
 public:
  void OnUserImageChanged(
      ash::personalization_app::mojom::UserImagePtr image) override {
    current_user_image_ = std::move(image);
  }

  void OnUserProfileImageUpdated(const GURL& profile_image) override {
    current_profile_image_ = profile_image;
  }

  void OnCameraPresenceCheckDone(bool is_camera_present) override {}

  void OnIsEnterpriseManagedChanged(bool is_enterprise_managed) override {
    is_enterprise_managed_ = is_enterprise_managed;
  }

  mojo::PendingRemote<ash::personalization_app::mojom::UserImageObserver>
  pending_remote() {
    DCHECK(!user_image_observer_receiver_.is_bound());
    return user_image_observer_receiver_.BindNewPipeAndPassRemote();
  }

  const ash::personalization_app::mojom::UserImage* current_user_image() {
    if (user_image_observer_receiver_.is_bound()) {
      user_image_observer_receiver_.FlushForTesting();
    }
    return current_user_image_.get();
  }

  const GURL& current_profile_image() {
    if (user_image_observer_receiver_.is_bound()) {
      user_image_observer_receiver_.FlushForTesting();
    }
    return current_profile_image_;
  }

  bool is_enterprise_managed() {
    if (user_image_observer_receiver_.is_bound()) {
      user_image_observer_receiver_.FlushForTesting();
    }
    return is_enterprise_managed_;
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::UserImageObserver>
      user_image_observer_receiver_{this};

  ash::personalization_app::mojom::UserImagePtr current_user_image_;
  GURL current_profile_image_;
  bool is_enterprise_managed_ = false;
};

// Blocks until LocalState is updated by UserManager.
class LocalStateUpdateWaiter : public user_manager::UserManager::Observer {
 public:
  LocalStateUpdateWaiter() {
    observation_.Observe(user_manager::UserManager::Get());
  }

  void Wait() { run_loop_.Run(); }

  void LocalStateChanged(user_manager::UserManager* user_manager) override {
    run_loop_.Quit();
  }

 private:
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      observation_{this};
  base::RunLoop run_loop_;
};

class FakeVideoCaptureService
    : public video_capture::mojom::VideoCaptureService {
 public:
  FakeVideoCaptureService() {
    content::OverrideVideoCaptureServiceForTesting(this);
  }

  ~FakeVideoCaptureService() override {
    content::OverrideVideoCaptureServiceForTesting(nullptr);
  }

  // mojom::VideoCaptureService:
  void InjectGpuDependencies(
      mojo::PendingRemote<video_capture::mojom::AcceleratorFactory>
          accelerator_factory) override {}

  void ConnectToCameraAppDeviceBridge(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver)
      override {}

  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory> receiver)
      override {}

  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override {
    fake_provider_.Bind(std::move(receiver));
  }

  void BindControlsForTesting(
      mojo::PendingReceiver<video_capture::mojom::TestingControls> receiver)
      override {}

 private:
  FakeVideoSourceProvider fake_provider_;
};

}  // namespace

class TestCameraImageDecoder
    : public PersonalizationAppUserProviderImpl::CameraImageDecoder {
 public:
  TestCameraImageDecoder() = default;
  ~TestCameraImageDecoder() override = default;

  void DecodeCameraImage(base::span<const uint8_t> encoded_bytes,
                         data_decoder::DecodeImageCallback callback) override {
    std::move(callback).Run(gfx::test::CreateBitmap(/*size=*/10));
  }
};

class PersonalizationAppUserProviderImplTest : public testing::Test {
 public:
  PersonalizationAppUserProviderImplTest() = default;
  PersonalizationAppUserProviderImplTest(
      const PersonalizationAppUserProviderImplTest&) = delete;
  PersonalizationAppUserProviderImplTest& operator=(
      const PersonalizationAppUserProviderImplTest&) = delete;
  ~PersonalizationAppUserProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    UserImageManagerImpl::SkipDefaultUserImageDownloadForTesting();
    ASSERT_TRUE(profile_manager_.SetUp());

    // Add a User then log in.
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId);
    user_manager_->AddUser(account_id);
    user_manager_->SaveUserDisplayName(
        account_id, base::UTF8ToUTF16(std::string(kFakeTestName)));
    user_manager_->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false, /*is_child=*/false);

    // Create a profile and set it as User profile.
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);
    user_manager_->OnUserProfileCreated(account_id, profile_->GetPrefs());

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());
    ash::UserImageManagerImpl::SkipProfileImageDownloadForTesting();
    user_provider_ =
        std::make_unique<PersonalizationAppUserProviderImpl>(&web_ui_);
    user_provider_->camera_image_decoder_ =
        std::make_unique<TestCameraImageDecoder>();

    user_provider_->BindInterface(
        user_provider_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    user_manager_->OnUserProfileWillBeDestroyed(
        AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId));
  }

  TestingProfile* profile() { return profile_; }

  content::TestWebUI* web_ui() { return &web_ui_; }

  mojo::Remote<ash::personalization_app::mojom::UserProvider>*
  user_provider_remote() {
    return &user_provider_remote_;
  }

  PersonalizationAppUserProviderImpl* user_provider() {
    return user_provider_.get();
  }

  gfx::ImageSkia user_image() {
    return user_manager::UserManager::Get()->GetActiveUser()->GetImage();
  }

  int image_index() {
    return user_manager::UserManager::Get()->GetActiveUser()->image_index();
  }

  ash::UserImageManagerImpl* user_image_manager() {
    return static_cast<ash::UserImageManagerImpl*>(
        ash::UserImageManagerRegistry::Get()->GetManager(
            GetAccountId(profile_)));
  }

  void SetUserImageObserver() {
    user_provider_remote_->SetUserImageObserver(
        test_user_image_observer_.pending_remote());
  }

  const ash::personalization_app::mojom::UserImage* current_user_image() {
    if (user_provider_remote_.is_bound()) {
      user_provider_remote_.FlushForTesting();
    }
    return test_user_image_observer_.current_user_image();
  }

  const GURL& current_profile_image() {
    if (user_provider_remote_.is_bound()) {
      user_provider_remote_.FlushForTesting();
    }
    return test_user_image_observer_.current_profile_image();
  }

  bool is_enterprise_managed() {
    if (user_provider_remote_.is_bound()) {
      user_provider_remote_.FlushForTesting();
    }
    return test_user_image_observer_.is_enterprise_managed();
  }

  void disable_customization_selectors() {
    profile()->GetPrefs()->SetBoolean(
        user_image::prefs::kUserAvatarCustomizationSelectorsEnabled, false);
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  base::FilePath GetTestFilePath() {
    base::FilePath test_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    return test_data_dir.AppendASCII("chromeos/avatars/avatar1.jpg");
  }

 private:
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  FakeVideoCaptureService fake_video_capture_service_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  UserImageManagerRegistry user_image_manager_registry_{user_manager_.Get()};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal(),
                                         &local_state_};
  data_decoder::test::InProcessDataDecoder data_decoder_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<TestingProfile> profile_;
  TestUserImageObserver test_user_image_observer_;
  mojo::Remote<ash::personalization_app::mojom::UserProvider>
      user_provider_remote_;
  std::unique_ptr<PersonalizationAppUserProviderImpl> user_provider_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PersonalizationAppUserProviderImplTest, GetsUserInfo) {
  user_provider_remote()->get()->GetUserInfo(base::BindLambdaForTesting(
      [](ash::personalization_app::UserDisplayInfo user_display_info) {
        EXPECT_EQ(kFakeTestEmail, user_display_info.email);
        EXPECT_EQ(kFakeTestName, user_display_info.name);
      }));
  user_provider_remote()->FlushForTesting();
}

TEST_F(PersonalizationAppUserProviderImplTest, ObservesUserAvatarImage) {
  // Observer has not received any images yet because it is not bound.
  EXPECT_EQ(nullptr, current_user_image());

  // Mock out profile image so the test does not try to download a real one.
  const gfx::ImageSkia& profile_image = gfx::test::CreateImageSkia(/*size=*/50);
  user_image_manager()->SetDownloadedProfileImageForTesting(profile_image);

  user_image_manager()->SaveUserImageFromProfileImage();
  SetUserImageObserver();

  // Observer received current user's avatar image.
  EXPECT_TRUE(current_user_image()->is_profile_image());

  // Select a default image.
  int image_index = ash::default_user_image::GetRandomDefaultImageIndex();
  user_image_manager()->SaveUserDefaultImageIndex(image_index);

  // Observer received the updated image information. Because it is a default
  // image, receives a default image with the right index.
  EXPECT_TRUE(current_user_image()->is_default_image());
  EXPECT_EQ(image_index, current_user_image()->get_default_image().index);
}

TEST_F(PersonalizationAppUserProviderImplTest, SelectDefaultImage) {
  SetUserImageObserver();

  // Select a default image.
  int image_index = ash::default_user_image::GetRandomDefaultImageIndex();
  user_provider_remote()->get()->SelectDefaultImage(image_index);
  user_provider_remote()->FlushForTesting();

  // Observer received the updated user image of type default with the right
  // index.
  EXPECT_TRUE(current_user_image()->is_default_image());
  EXPECT_EQ(image_index, current_user_image()->get_default_image().index);
}

TEST_F(PersonalizationAppUserProviderImplTest, ObservesUserProfileImage) {
  // Observer has not received any images yet because it is not bound.
  EXPECT_EQ(GURL(), current_profile_image());

  SetUserImageObserver();

  // Select a profile image.
  gfx::ImageSkia profile_image = gfx::test::CreateImageSkia(/*size=*/50);
  user_image_manager()->SetDownloadedProfileImageForTesting(profile_image);
  user_image_manager()->SaveUserImageFromProfileImage();

  // Observer received the updated profile image url.
  EXPECT_EQ(GURL(webui::GetBitmapDataUrl(*profile_image.bitmap())),
            current_profile_image());
}

TEST_F(PersonalizationAppUserProviderImplTest, SelectProfileImage) {
  SetUserImageObserver();

  gfx::ImageSkia profile_image = gfx::test::CreateImageSkia(/*size=*/50);
  user_image_manager()->SetDownloadedProfileImageForTesting(profile_image);
  user_provider_remote()->get()->SelectProfileImage();
  user_provider_remote()->FlushForTesting();

  EXPECT_EQ(GURL(webui::GetBitmapDataUrl(*profile_image.bitmap())),
            current_profile_image());
}

TEST_F(PersonalizationAppUserProviderImplTest, EncodesUserImageToPngBuffer) {
  SetUserImageObserver();

  gfx::ImageSkia test_image = gfx::test::CreateImageSkia(/*size=*/4);
  test_image.MakeThreadSafe();

  // Save a jpg user image. This will trigger the image encoding to png path.
  user_image_manager()->SaveUserImage(std::make_unique<user_manager::UserImage>(
      test_image, base::MakeRefCounted<base::RefCountedBytes>(),
      user_manager::UserImage::ImageFormat::FORMAT_JPEG));

  EXPECT_TRUE(current_user_image()->is_external_image());

  auto encoded_png = base::MakeRefCounted<base::RefCountedBytes>(
      current_user_image()->get_external_image());

  std::vector<unsigned char> expected_data;
  ASSERT_TRUE(gfx::PNGCodec::EncodeBGRASkBitmap(
      *test_image.bitmap(), /*discard_transparency=*/false, &expected_data));

  // The BigBuffer data received from the observer should be equal to the test
  // image encoded to png.
  ASSERT_GT(expected_data.size(), 0u);
  EXPECT_EQ(expected_data, base::span(*encoded_png));
}

TEST_F(PersonalizationAppUserProviderImplTest,
       RecordsDefaultImageChangeHistogram) {
  int image_index = 55;
  ASSERT_TRUE(ash::default_user_image::IsInCurrentImageSet(image_index));

  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::UserImageManagerImpl::ImageIndexToHistogramIndex(image_index), 0);

  user_provider_remote()->get()->SelectDefaultImage(image_index);
  user_provider_remote()->FlushForTesting();

  // Bucket count is incremented after selecting this default image.
  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::UserImageManagerImpl::ImageIndexToHistogramIndex(image_index), 1);

  // Select the same image again.
  user_provider_remote()->get()->SelectDefaultImage(image_index);
  user_provider_remote()->FlushForTesting();

  // Bucket count is not incremented.
  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::UserImageManagerImpl::ImageIndexToHistogramIndex(image_index), 1);
}

TEST_F(PersonalizationAppUserProviderImplTest,
       RecordsCameraImageChangeHistogram) {
  // No camera images recorded yet.
  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageFromCamera, 0);

  user_provider_remote()->get()->SelectCameraImage(FakeEncodedPngBuffer());
  user_provider_remote()->FlushForTesting();

  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageFromCamera, 1);

  user_provider_remote()->get()->SelectCameraImage(FakeEncodedPngBuffer());
  user_provider_remote()->FlushForTesting();

  // Every camera image increments the count.
  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageFromCamera, 2);
}

TEST_F(PersonalizationAppUserProviderImplTest,
       RecordsProfileImageChangeHistogram) {
  // Set a fake profile image to skip trying to downloading one.
  const gfx::ImageSkia& profile_image = gfx::test::CreateImageSkia(/*size=*/50);
  user_image_manager()->SetDownloadedProfileImageForTesting(profile_image);

  // No profile image recorded yet.
  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageFromProfile, 0);

  // Select a default image first to make sure profile is not selected.
  user_provider_remote()->get()->SelectDefaultImage(
      ash::default_user_image::GetRandomDefaultImageIndex());
  // Now select profile.
  user_provider_remote()->get()->SelectProfileImage();
  user_provider_remote()->FlushForTesting();

  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageFromProfile, 1);

  // Selecting profile image again is a no-op.
  user_provider_remote()->get()->SelectProfileImage();
  user_provider_remote()->FlushForTesting();

  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageFromProfile, 1);
}

TEST_F(PersonalizationAppUserProviderImplTest,
       RecordsSelectLastExternalUserImageChangeHistogram) {
  // No external image recorded yet.
  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageExternal, 0);

  SetUserImageObserver();

  // Set up a camera image as last external and then select a default.
  user_provider_remote()->get()->SelectCameraImage(FakeEncodedPngBuffer());
  user_provider_remote()->FlushForTesting();
  EXPECT_TRUE(current_user_image()->is_external_image());

  user_provider_remote()->get()->SelectDefaultImage(
      ash::default_user_image::GetRandomDefaultImageIndex());
  user_provider_remote()->FlushForTesting();

  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageExternal, 0);

  // A default user image is selected.
  ASSERT_TRUE(ash::default_user_image::IsInCurrentImageSet(image_index()));

  user_provider_remote()->get()->SelectLastExternalUserImage();
  user_provider_remote()->FlushForTesting();

  // Finally records an external image chosen.
  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageExternal, 1);
}

TEST_F(PersonalizationAppUserProviderImplTest, SetsUserImageManagedByPolicy) {
  base::SequenceCheckerImpl::EnableStackLogging();
  // Make sure image does not start managed.
  ASSERT_FALSE(user_image_manager()->IsUserImageManaged());

  SetUserImageObserver();
  ASSERT_FALSE(is_enterprise_managed());

  user_image_manager()->OnExternalDataSet(policy::key::kUserAvatarImage);
  EXPECT_TRUE(is_enterprise_managed());

  user_image_manager()->OnExternalDataCleared(policy::key::kUserAvatarImage);
  EXPECT_FALSE(is_enterprise_managed());

  // Changes back to enterprise managed.
  user_image_manager()->OnExternalDataSet(policy::key::kUserAvatarImage);
  EXPECT_TRUE(is_enterprise_managed());
}

TEST_F(PersonalizationAppUserProviderImplTest,
       SendsDeprecatedAuthorAndWebsite) {
  constexpr int kDeprecatedImageWithSourceInfoIndex = 19;
  user_image_manager()->SaveUserDefaultImageIndex(
      kDeprecatedImageWithSourceInfoIndex);
  SetUserImageObserver();

  std::optional<default_user_image::DeprecatedSourceInfo> expected_source_info =
      default_user_image::GetDeprecatedDefaultImageSourceInfo(
          kDeprecatedImageWithSourceInfoIndex);
  ASSERT_TRUE(expected_source_info.has_value())
      << "Image index " << kDeprecatedImageWithSourceInfoIndex
      << " must have associated source info";

  EXPECT_TRUE(current_user_image()->is_default_image());

  EXPECT_EQ(expected_source_info->author,
            current_user_image()->get_default_image().source_info->author);
  EXPECT_EQ(expected_source_info->website,
            current_user_image()->get_default_image().source_info->website);
}

TEST_F(PersonalizationAppUserProviderImplTest, SelectProfileImageDisabled) {
  mojo::test::BadMessageObserver bad_message_observer;
  disable_customization_selectors();

  user_provider_remote()->get()->SelectProfileImage();
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Not allowed to select profile image");
}

TEST_F(PersonalizationAppUserProviderImplTest, SelectCameraImageDisabled) {
  mojo::test::BadMessageObserver bad_message_observer;
  disable_customization_selectors();

  user_provider_remote()->get()->SelectCameraImage(FakeEncodedPngBuffer());

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Not allowed to select camera image");
}

TEST_F(PersonalizationAppUserProviderImplTest,
       SelectLastExternalUserImageDisabled) {
  mojo::test::BadMessageObserver bad_message_observer;
  disable_customization_selectors();

  user_provider_remote()->get()->SelectLastExternalUserImage();

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Not allowed to select last external image");
}

TEST_F(PersonalizationAppUserProviderImplTest, SelectImageFromDiskDisabled) {
  mojo::test::BadMessageObserver bad_message_observer;
  disable_customization_selectors();

  user_provider_remote()->get()->SelectImageFromDisk();

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "Not allowed to select image file");
}

TEST_F(PersonalizationAppUserProviderImplTest, SelectImageFromDisk) {
  ASSERT_EQ(user_manager::UserManager::Get()
                ->FindUser(GetAccountId(profile()))
                ->image_index(),
            user_manager::UserImage::Type::kProfile);

  base::FilePath file_path = GetTestFilePath();
  auto fake_file_selector =
      std::make_unique<ash::FakeUserImageFileSelector>(web_ui());
  fake_file_selector->SetFilePath(file_path);
  user_provider()->SetUserImageFileSelectorForTesting(
      std::move(fake_file_selector));
  user_provider_remote()->get()->SelectImageFromDisk();
  LocalStateUpdateWaiter().Wait();
  EXPECT_EQ(user_manager::UserManager::Get()
                ->FindUser(GetAccountId(profile()))
                ->image_index(),
            user_manager::UserImage::Type::kExternal);
}

TEST_F(PersonalizationAppUserProviderImplTest,
       RecordsSelectImageFromDiskChangeHistogram) {
  // No external image set yet.
  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageExternal, 0);

  base::FilePath file_path = GetTestFilePath();
  auto fake_file_selector =
      std::make_unique<ash::FakeUserImageFileSelector>(web_ui());
  fake_file_selector->SetFilePath(file_path);
  user_provider()->SetUserImageFileSelectorForTesting(
      std::move(fake_file_selector));
  user_provider_remote()->get()->SelectImageFromDisk();
  user_provider_remote()->FlushForTesting();

  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageExternal, 1);

  user_provider_remote()->get()->SelectImageFromDisk();
  user_provider_remote()->FlushForTesting();

  // Incremented every time a file is selected.
  histogram_tester().ExpectBucketCount(
      ash::UserImageManagerImpl::kUserImageChangedHistogramName,
      ash::default_user_image::kHistogramImageExternal, 2);
}

}  // namespace ash::personalization_app
