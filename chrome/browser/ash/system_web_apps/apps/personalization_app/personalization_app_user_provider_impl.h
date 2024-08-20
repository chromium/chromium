// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_USER_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_USER_PROVIDER_IMPL_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_user_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/users/avatar/user_image_file_selector.h"
#include "chromeos/ash/components/camera_presence_notifier/camera_presence_notifier.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class WebUI;
}  // namespace content

namespace ash::personalization_app {

class PersonalizationAppUserProviderImpl
    : public PersonalizationAppUserProvider,
      public user_manager::UserManager::Observer {
 public:
  explicit PersonalizationAppUserProviderImpl(content::WebUI* web_ui);

  PersonalizationAppUserProviderImpl(
      const PersonalizationAppUserProviderImpl&) = delete;
  PersonalizationAppUserProviderImpl& operator=(
      const PersonalizationAppUserProviderImpl&) = delete;

  ~PersonalizationAppUserProviderImpl() override;

  // PersonalizationAppUserProvider:
  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::UserProvider>
          receiver) override;

  // personalization_app::mojom::UserProvider:
  void SetUserImageObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::UserImageObserver>
          observer) override;

  void GetUserInfo(GetUserInfoCallback callback) override;

  // This function is called when a user navigates to the page to change
  // Avatar image. Therefore use it to track if the user has seen the change
  // avatar page for Personalization HaTS.
  void GetDefaultUserImages(GetDefaultUserImagesCallback callback) override;

  bool IsCustomizationSelectorsPrefEnabled() override;

  void SelectImageFromDisk() override;

  void SelectDefaultImage(int index) override;

  void SelectProfileImage() override;

  void SelectCameraImage(::mojo_base::BigBuffer data) override;

  void SelectLastExternalUserImage() override;

  void OnFileSelected(const base::FilePath& path);

  // user_manager::UserManager::Observer:
  void OnUserImageChanged(const user_manager::User& user) override;

  void OnUserImageIsEnterpriseManagedChanged(
      const user_manager::User& user,
      bool is_enterprise_managed) override;

  void OnUserProfileImageUpdated(const user_manager::User& user,
                                 const gfx::ImageSkia& profile_image) override;

  void OnCameraPresenceCheckDone(bool is_camera_present);

  void SetUserImageFileSelectorForTesting(
      std::unique_ptr<ash::UserImageFileSelector> file_selector);

 private:
  friend class PersonalizationAppUserProviderImplTest;
  friend class TestCameraImageDecoder;

  // A class to decode camera images. Mocked out in tests.
  class CameraImageDecoder {
   public:
    CameraImageDecoder();
    virtual ~CameraImageDecoder();

    virtual void DecodeCameraImage(base::span<const uint8_t> encoded_bytes,
                                   data_decoder::DecodeImageCallback callback);

   private:
    data_decoder::DataDecoder data_decoder_;
  };

  void OnCameraImageDecoded(scoped_refptr<base::RefCountedBytes> photo_bytes,
                            const SkBitmap& decoded_bitmap);

  void OnExternalUserImageEncoded(std::vector<unsigned char> encoded_png);

  void UpdateUserImageObserver(
      ash::personalization_app::mojom::UserImagePtr user_image);

  // Pointer to profile of user that opened personalization SWA. Not owned.
  raw_ptr<Profile> profile_ = nullptr;

  // Flag to track whether the user viewed the user subpage.
  bool page_viewed_ = false;

  std::unique_ptr<user_manager::UserImage> last_external_user_image_;

  std::unique_ptr<CameraImageDecoder> camera_image_decoder_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      user_manager_observer_{this};

  std::unique_ptr<ash::CameraPresenceNotifier> camera_presence_notifier_;

  mojo::Remote<ash::personalization_app::mojom::UserImageObserver>
      user_image_observer_remote_;

  mojo::Receiver<ash::personalization_app::mojom::UserProvider> user_receiver_{
      this};

  scoped_refptr<base::SequencedTaskRunner> image_encoding_task_runner_;

  std::unique_ptr<ash::UserImageFileSelector> user_image_file_selector_;

  base::WeakPtrFactory<PersonalizationAppUserProviderImpl> weak_ptr_factory_{
      this};

  base::WeakPtrFactory<PersonalizationAppUserProviderImpl>
      image_encoding_weak_ptr_factory_{this};

  base::WeakPtrFactory<PersonalizationAppUserProviderImpl>
      image_decode_weak_ptr_factory_{this};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_USER_PROVIDER_IMPL_H_
