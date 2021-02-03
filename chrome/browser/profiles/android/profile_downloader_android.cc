// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "base/memory/checked_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_downloader.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/services/android/jni_headers/ProfileDownloader_jni.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// An account fetcher callback.
class AccountInfoRetriever : public ProfileDownloaderDelegate {
 public:
  AccountInfoRetriever(Profile* profile,
                       const CoreAccountInfo& core_account_info,
                       const int desired_image_side_pixels)
      : profile_(profile),
        core_account_info_(core_account_info),
        desired_image_side_pixels_(desired_image_side_pixels) {}

  void Start() {
    profile_image_downloader_.reset(new ProfileDownloader(this));
    profile_image_downloader_->StartForAccount(core_account_info_.account_id);
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

  std::string GetCachedPictureURL() const override { return std::string(); }

  bool IsPreSignin() const override { return true; }

  void OnProfileDownloadSuccess(ProfileDownloader* downloader) override {
    base::string16 full_name = downloader->GetProfileFullName();
    base::string16 given_name = downloader->GetProfileGivenName();
    SkBitmap bitmap = downloader->GetProfilePicture();
    ScopedJavaLocalRef<jobject> jbitmap;
    if (!bitmap.isNull() && bitmap.bytesPerPixel() != 0)
      jbitmap = gfx::ConvertToJavaBitmap(bitmap);

    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ProfileDownloader_onProfileDownloadSuccess(
        env,
        base::android::ConvertUTF8ToJavaString(env, core_account_info_.email),
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
  CheckedPtr<Profile> profile_;

  // The account info of account to be loaded.
  const CoreAccountInfo core_account_info_;

  // Desired side length of the profile image (in pixels).
  const int desired_image_side_pixels_;

  DISALLOW_COPY_AND_ASSIGN(AccountInfoRetriever);
};

}  // namespace

// static
void JNI_ProfileDownloader_StartFetchingAccountInfoFor(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& jcore_account_info,
    jint image_side_pixels) {
  AccountInfoRetriever* retriever = new AccountInfoRetriever(
      ProfileAndroid::FromProfileAndroid(jprofile),
      ConvertFromJavaCoreAccountInfo(env, jcore_account_info),
      image_side_pixels);
  retriever->Start();
}
