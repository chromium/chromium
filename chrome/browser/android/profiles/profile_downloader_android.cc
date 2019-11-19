// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "chrome/android/chrome_jni_headers/ProfileDownloader_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image_skia.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// An account fetcher callback.
class AccountInfoRetriever : public ProfileDownloaderDelegate {
 public:
  AccountInfoRetriever(Profile* profile,
                       const CoreAccountId& account_id,
                       const std::string& email,
                       const int desired_image_side_pixels,
                       bool is_pre_signin)
      : profile_(profile),
        account_id_(account_id),
        email_(email),
        desired_image_side_pixels_(desired_image_side_pixels),
        is_pre_signin_(is_pre_signin) {}

  void Start() {
    profile_image_downloader_.reset(new ProfileDownloader(this));
    profile_image_downloader_->StartForAccount(account_id_);
  }

 private:
  void Shutdown() {
    profile_image_downloader_.reset();
    delete this;
  }

  // ProfileDownloaderDelegate implementation:
  bool NeedsProfilePicture() const override {
    return desired_image_side_pixels_ > 0;
  }

  int GetDesiredImageSideLength() const override {
    return desired_image_side_pixels_;
  }

  signin::IdentityManager* GetIdentityManager() override {
    return IdentityManagerFactory::GetForProfile(profile_);
  }

  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override {
    return content::BrowserContext::GetDefaultStoragePartition(profile_)
        ->GetURLLoaderFactoryForBrowserProcess()
        .get();
  }

  std::string GetCachedPictureURL() const override {
    return std::string();
  }

  bool IsPreSignin() const override {
    return is_pre_signin_;
  }

  void OnProfileDownloadSuccess(
      ProfileDownloader* downloader) override {
    base::string16 full_name = downloader->GetProfileFullName();
    base::string16 given_name = downloader->GetProfileGivenName();
    SkBitmap bitmap = downloader->GetProfilePicture();
    ScopedJavaLocalRef<jobject> jbitmap;
    if (!bitmap.isNull() && bitmap.bytesPerPixel() != 0)
      jbitmap = gfx::ConvertToJavaBitmap(&bitmap);

    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ProfileDownloader_onProfileDownloadSuccess(
        env, base::android::ConvertUTF8ToJavaString(env, email_),
        base::android::ConvertUTF16ToJavaString(env, full_name),
        base::android::ConvertUTF16ToJavaString(env, given_name), jbitmap);
    Shutdown();
  }

  void OnProfileDownloadFailure(
      ProfileDownloader* downloader,
      ProfileDownloaderDelegate::FailureReason reason) override {
    LOG(ERROR) << "Failed to download the profile information: " << reason;
    Shutdown();
  }

  // The profile image downloader instance.
  std::unique_ptr<ProfileDownloader> profile_image_downloader_;

  // The browser profile associated with this download request.
  Profile* profile_;

  // The account ID and email address of account to be loaded.
  const CoreAccountId account_id_;
  const std::string email_;

  // Desired side length of the profile image (in pixels).
  const int desired_image_side_pixels_;

  // True when the profile download is happening before the user has signed in,
  // such as during first run when we can still get tokens and want to fetch
  // the profile name and picture to display.
  bool is_pre_signin_;

  DISALLOW_COPY_AND_ASSIGN(AccountInfoRetriever);
};

}  // namespace

// static
ScopedJavaLocalRef<jstring>
JNI_ProfileDownloader_GetCachedFullNameForPrimaryAccount(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

  base::string16 name;
  ProfileAttributesEntry* entry;
  if (g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    name = entry->GetGAIAName();
  }

  return base::android::ConvertUTF16ToJavaString(env, name);
}

// static
ScopedJavaLocalRef<jstring>
JNI_ProfileDownloader_GetCachedGivenNameForPrimaryAccount(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

  base::string16 name;
  ProfileAttributesEntry* entry;
  if (g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    name = entry->GetGAIAGivenName();
  }

  return base::android::ConvertUTF16ToJavaString(env, name);
}

// static
ScopedJavaLocalRef<jobject>
JNI_ProfileDownloader_GetCachedAvatarForPrimaryAccount(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

  ScopedJavaLocalRef<jobject> jbitmap;
  ProfileAttributesEntry* entry;
  if (g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    gfx::Image avatar_image = entry->GetAvatarIcon();
    if (!avatar_image.IsEmpty() &&
        avatar_image.Width() > profiles::kAvatarIconSize &&
        avatar_image.Height() > profiles::kAvatarIconSize &&
        avatar_image.AsImageSkia().bitmap()) {
      jbitmap = gfx::ConvertToJavaBitmap(avatar_image.AsImageSkia().bitmap());
    }
  }

  return jbitmap;
}

// static
void JNI_ProfileDownloader_StartFetchingAccountInfoFor(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jemail,
    jint image_side_pixels,
    jboolean is_pre_signin) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  const std::string email =
      base::android::ConvertJavaStringToUTF8(env, jemail);

  auto maybe_account_info =
      IdentityManagerFactory::GetForProfile(profile)
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              email);

  if (!maybe_account_info.has_value()) {
    LOG(ERROR) << "Attempted to get AccountInfo for account not in the "
               << "IdentityManager";
    return;
  }

  AccountInfoRetriever* retriever =
      new AccountInfoRetriever(profile, maybe_account_info.value().account_id,
                               email, image_side_pixels, is_pre_signin);
  retriever->Start();
}
