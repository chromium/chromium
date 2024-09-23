// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_user_provider_impl.h"

#include "ash/public/cpp/default_user_image.h"
#include "ash/public/cpp/personalization_app/user_display_info.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/login/users/avatar/user_image_file_selector.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/avatar/user_image_prefs.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/camera_presence_notifier/camera_presence_notifier.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash::personalization_app {

namespace {

using ash::personalization_app::GetAccountId;
using ash::personalization_app::GetUser;

// Called on |image_encoding_task_runner_| sequence.
std::vector<unsigned char> ImageSkiaToPngBytes(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    return {};
  }

  // Encode the image as png.
  std::vector<unsigned char> output;
  if (gfx::PNGCodec::EncodeBGRASkBitmap(*image.bitmap(),
                                        /*discard_transparency=*/false,
                                        &output)) {
    return output;
  }

  // Return empty vector if case encoding failed.
  return {};
}

}  // namespace

PersonalizationAppUserProviderImpl::CameraImageDecoder::CameraImageDecoder() =
    default;

PersonalizationAppUserProviderImpl::CameraImageDecoder::~CameraImageDecoder() =
    default;

void PersonalizationAppUserProviderImpl::CameraImageDecoder::DecodeCameraImage(
    base::span<const uint8_t> encoded_bytes,
    data_decoder::DecodeImageCallback callback) {
  data_decoder::DecodeImage(
      &data_decoder_, encoded_bytes, data_decoder::mojom::ImageCodec::kPng,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
      /*desired_image_frame_size=*/gfx::Size(), std::move(callback));
}

PersonalizationAppUserProviderImpl::PersonalizationAppUserProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)),
      camera_image_decoder_(
          std::make_unique<
              PersonalizationAppUserProviderImpl::CameraImageDecoder>()),
      image_encoding_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      user_image_file_selector_(
          std::make_unique<ash::UserImageFileSelector>(web_ui)) {
  camera_presence_notifier_ =
      std::make_unique<CameraPresenceNotifier>(base::BindRepeating(
          &PersonalizationAppUserProviderImpl::OnCameraPresenceCheckDone,
          weak_ptr_factory_.GetWeakPtr()));
}

PersonalizationAppUserProviderImpl::~PersonalizationAppUserProviderImpl() {
  if (page_viewed_) {
    ::ash::personalization_app::PersonalizationAppManagerFactory::
        GetForBrowserContext(profile_)
            ->MaybeStartHatsTimer(
                ::ash::personalization_app::HatsSurveyType::kAvatar);
  }
}

void PersonalizationAppUserProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::UserProvider>
        receiver) {
  user_receiver_.reset();
  user_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppUserProviderImpl::SetUserImageObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::UserImageObserver>
        observer) {
  // May already be bound if user refreshes page.
  user_image_observer_remote_.reset();
  user_image_observer_remote_.Bind(std::move(observer));
  DCHECK(user_manager::UserManager::IsInitialized());
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager_observer_.IsObserving()) {
    user_manager_observer_.Observe(user_manager);
  }

  const auto* user = GetUser(profile_);

  // Call observers manually the first time to initialize state.
  OnUserImageChanged(*user);

  ash::UserImageManagerImpl* user_image_manager =
      ash::UserImageManagerRegistry::Get()->GetManager(GetAccountId(profile_));
  const gfx::ImageSkia& profile_image =
      user_image_manager->DownloadedProfileImage();
  OnUserProfileImageUpdated(*user, profile_image);
  OnUserImageIsEnterpriseManagedChanged(
      *user, user_image_manager->IsUserImageManaged());

  camera_presence_notifier_->Start();
}

void PersonalizationAppUserProviderImpl::GetUserInfo(
    GetUserInfoCallback callback) {
  const user_manager::User* user = GetUser(profile_);
  DCHECK(user);
  std::move(callback).Run(ash::personalization_app::UserDisplayInfo(*user));
}

void PersonalizationAppUserProviderImpl::GetDefaultUserImages(
    GetDefaultUserImagesCallback callback) {
  page_viewed_ = true;
  std::vector<ash::default_user_image::DefaultUserImage> images =
      ash::default_user_image::GetCurrentImageSet();
  std::move(callback).Run(std::move(images));
}

void PersonalizationAppUserProviderImpl::SelectImageFromDisk() {
  if (!IsCustomizationSelectorsPrefEnabled()) {
    user_receiver_.ReportBadMessage("Not allowed to select image file");
    return;
  }
  user_image_file_selector_->SelectFile(
      base::BindOnce(&PersonalizationAppUserProviderImpl::OnFileSelected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

void PersonalizationAppUserProviderImpl::SelectDefaultImage(int index) {
  if (!ash::default_user_image::IsInCurrentImageSet(index)) {
    user_receiver_.ReportBadMessage("Invalid user image selected");
    return;
  }

  if (GetUser(profile_)->image_index() != index) {
    ash::UserImageManagerImpl::RecordUserImageChanged(
        ash::UserImageManagerImpl::ImageIndexToHistogramIndex(index));
  }

  auto* user_image_manager =
      ash::UserImageManagerRegistry::Get()->GetManager(GetAccountId(profile_));

  user_image_manager->SaveUserDefaultImageIndex(index);
}

void PersonalizationAppUserProviderImpl::SelectProfileImage() {
  if (!IsCustomizationSelectorsPrefEnabled()) {
    user_receiver_.ReportBadMessage("Not allowed to select profile image");
    return;
  }

  if (GetUser(profile_)->image_index() !=
      user_manager::UserImage::Type::kProfile) {
    ash::UserImageManagerImpl::RecordUserImageChanged(
        ash::default_user_image::kHistogramImageFromProfile);
  }

  ash::UserImageManagerImpl* user_image_manager =
      ash::UserImageManagerRegistry::Get()->GetManager(GetAccountId(profile_));

  user_image_manager->SaveUserImageFromProfileImage();
}

void PersonalizationAppUserProviderImpl::SelectCameraImage(
    ::mojo_base::BigBuffer data) {
  if (!IsCustomizationSelectorsPrefEnabled()) {
    user_receiver_.ReportBadMessage("Not allowed to select camera image");
    return;
  }
  // Make a copy of the data.
  auto ref_counted = base::MakeRefCounted<base::RefCountedBytes>(data);
  // Get a view of the same data copied above.
  auto as_span = base::make_span(ref_counted->front(), ref_counted->size());

  camera_image_decoder_->DecodeCameraImage(
      as_span,
      base::BindOnce(&PersonalizationAppUserProviderImpl::OnCameraImageDecoded,
                     image_decode_weak_ptr_factory_.GetWeakPtr(),
                     std::move(ref_counted)));
}

void PersonalizationAppUserProviderImpl::SelectLastExternalUserImage() {
  if (!IsCustomizationSelectorsPrefEnabled()) {
    user_receiver_.ReportBadMessage(
        "Not allowed to select last external image");
    return;
  }

  if (!last_external_user_image_) {
    LOG(WARNING) << "No last external user image present";
    return;
  }

  if (GetUser(profile_)->image_index() !=
      user_manager::UserImage::Type::kExternal) {
    ash::UserImageManagerImpl::RecordUserImageChanged(
        ash::default_user_image::kHistogramImageExternal);
  }

  ash::UserImageManagerImpl* user_image_manager =
      ash::UserImageManagerRegistry::Get()->GetManager(GetAccountId(profile_));

  user_image_manager->SaveUserImage(std::move(last_external_user_image_));
}

void PersonalizationAppUserProviderImpl::OnFileSelected(
    const base::FilePath& path) {
  // No way to tell if this is a different external image than last time, so
  // always record it.
  ash::UserImageManagerImpl::RecordUserImageChanged(
      ash::default_user_image::kHistogramImageExternal);

  ash::UserImageManagerImpl* user_image_manager =
      ash::UserImageManagerRegistry::Get()->GetManager(GetAccountId(profile_));

  user_image_manager->SaveUserImageFromFile(path);
}

void PersonalizationAppUserProviderImpl::OnUserImageChanged(
    const user_manager::User& user) {
  const user_manager::User* desired_user = GetUser(profile_);
  DCHECK(desired_user);

  if (user.GetAccountId() != desired_user->GetAccountId()) {
    return;
  }

  // Cancel requests to encode and send external user image data because it is
  // now out of date.
  image_encoding_weak_ptr_factory_.InvalidateWeakPtrs();

  int image_index = desired_user->image_index();
  switch (image_index) {
    case user_manager::UserImage::Type::kInvalid: {
      UpdateUserImageObserver(
          ash::personalization_app::mojom::UserImage::NewInvalidImage(
              ash::personalization_app::mojom::InvalidImage::New()));
      break;
    }
    case user_manager::UserImage::Type::kExternal: {
      if (desired_user->image_format() == user_manager::UserImage::FORMAT_PNG &&
          desired_user->has_image_bytes()) {
        last_external_user_image_ = std::make_unique<user_manager::UserImage>(
            desired_user->GetImage(), desired_user->image_bytes(),
            user_manager::UserImage::ImageFormat::FORMAT_PNG);

        // No need to re-encode a png because already have encoded bytes.
        auto image_bytes = desired_user->image_bytes();
        UpdateUserImageObserver(
            ash::personalization_app::mojom::UserImage::NewExternalImage(
                mojo_base::BigBuffer(base::make_span(image_bytes->front(),
                                                     image_bytes->size()))));
      } else {
        // Defer saving |last_external_user_image| until it has been encoded to
        // png bytes.
        DCHECK(desired_user->GetImage().IsThreadSafe())
            << "User image loader marks user images as thread safe";
        image_encoding_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&ImageSkiaToPngBytes, desired_user->GetImage()),
            base::BindOnce(
                &PersonalizationAppUserProviderImpl::OnExternalUserImageEncoded,
                image_encoding_weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    }
    case user_manager::UserImage::Type::kProfile: {
      UpdateUserImageObserver(
          ash::personalization_app::mojom::UserImage::NewProfileImage(
              ash::personalization_app::mojom::ProfileImage::New()));
      break;
    }
    default: {
      if (!ash::default_user_image::IsValidIndex(image_index)) {
        LOG(ERROR) << "Invalid image index received";
        break;
      }
      UpdateUserImageObserver(
          ash::personalization_app::mojom::UserImage::NewDefaultImage(
              ash::default_user_image::GetDefaultUserImage(image_index)));
      break;
    }
  }
}

void PersonalizationAppUserProviderImpl::OnUserImageIsEnterpriseManagedChanged(
    const user_manager::User& user,
    bool is_enterprise_managed) {
  if (user.GetAccountId() != GetUser(profile_)->GetAccountId()) {
    return;
  }

  user_image_observer_remote_->OnIsEnterpriseManagedChanged(
      is_enterprise_managed);
}

void PersonalizationAppUserProviderImpl::OnUserProfileImageUpdated(
    const user_manager::User& user,
    const gfx::ImageSkia& profile_image) {
  const user_manager::User* desired_user = GetUser(profile_);
  DCHECK(desired_user);

  if (user.GetAccountId() != desired_user->GetAccountId()) {
    return;
  }

  user_image_observer_remote_->OnUserProfileImageUpdated(
      profile_image.isNull()
          ? GURL()
          : GURL(webui::GetBitmapDataUrl(*profile_image.bitmap())));
}

void PersonalizationAppUserProviderImpl::OnCameraPresenceCheckDone(
    bool is_camera_present) {
  user_image_observer_remote_->OnCameraPresenceCheckDone(is_camera_present);
}

void PersonalizationAppUserProviderImpl::OnCameraImageDecoded(
    scoped_refptr<base::RefCountedBytes> bytes,
    const SkBitmap& decoded_bitmap) {
  if (decoded_bitmap.isNull()) {
    LOG(WARNING) << "Camera image failed decoding";
    return;
  }

  // Every time we decode a camera image it is new, so always record a metric
  // here.
  ash::UserImageManagerImpl::RecordUserImageChanged(
      ash::default_user_image::kHistogramImageFromCamera);

  auto user_image = std::make_unique<user_manager::UserImage>(
      gfx::ImageSkia::CreateFrom1xBitmap(decoded_bitmap), std::move(bytes),
      user_manager::UserImage::ImageFormat::FORMAT_PNG);
  // Image was successfully decoded so it is valid png data.
  user_image->MarkAsSafe();

  auto* user_image_manager =
      ash::UserImageManagerRegistry::Get()->GetManager(GetAccountId(profile_));

  user_image_manager->SaveUserImage(std::move(user_image));
}

void PersonalizationAppUserProviderImpl::OnExternalUserImageEncoded(
    std::vector<unsigned char> encoded_png) {
  const user_manager::User* user = GetUser(profile_);

  // Since we did the work of encoding user image to png bytes, save it now.
  // Makes a copy of |encoded_png|.
  last_external_user_image_ = std::make_unique<user_manager::UserImage>(
      user->GetImage(),
      base::MakeRefCounted<base::RefCountedBytes>(encoded_png),
      user_manager::UserImage::ImageFormat::FORMAT_PNG);

  UpdateUserImageObserver(
      ash::personalization_app::mojom::UserImage::NewExternalImage(
          mojo_base::BigBuffer(std::move(encoded_png))));
}

bool PersonalizationAppUserProviderImpl::IsCustomizationSelectorsPrefEnabled() {
  return user_image::prefs::IsCustomizationSelectorsPrefEnabled(
      profile_->GetPrefs());
}

void PersonalizationAppUserProviderImpl::UpdateUserImageObserver(
    ash::personalization_app::mojom::UserImagePtr user_image) {
  user_image_observer_remote_->OnUserImageChanged(std::move(user_image));
}

void PersonalizationAppUserProviderImpl::SetUserImageFileSelectorForTesting(
    std::unique_ptr<ash::UserImageFileSelector> file_selector) {
  user_image_file_selector_ = std::move(file_selector);
}
}  // namespace ash::personalization_app
