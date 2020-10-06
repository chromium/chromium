// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_install_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/vr/android_vr_utils.h"
#include "chrome/browser/android/vr/ar_jni_headers/ArCoreInstallUtils_jni.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"
#include "chrome/browser/android/vr/xr_install_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "device/vr/android/arcore/arcore_device_provider_factory.h"

using base::android::AttachCurrentThread;

namespace vr {

namespace {
class ArCoreDeviceProviderFactoryImpl
    : public device::ArCoreDeviceProviderFactory {
 public:
  ArCoreDeviceProviderFactoryImpl() = default;
  ~ArCoreDeviceProviderFactoryImpl() override = default;
  std::unique_ptr<device::VRDeviceProvider> CreateDeviceProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArCoreDeviceProviderFactoryImpl);
};

std::unique_ptr<device::VRDeviceProvider>
ArCoreDeviceProviderFactoryImpl::CreateDeviceProvider() {
  return std::make_unique<device::ArCoreDeviceProvider>();
}

}  // namespace

ArCoreInstallHelper::ArCoreInstallHelper() {
  // As per documentation, it's recommended to issue a call to
  // ArCoreApk.checkAvailability() early in application lifecycle & ignore the
  // result so that subsequent calls can return cached result:
  // https://developers.google.com/ar/develop/java/enable-arcore
  // In the event that a remote call is required, it will not block on that
  // remote call per:
  // https://developers.google.com/ar/reference/java/arcore/reference/com/google/ar/core/ArCoreApk#checkAvailability
  Java_ArCoreInstallUtils_shouldRequestInstallSupportedArCore(
      AttachCurrentThread());

  java_install_utils_ = Java_ArCoreInstallUtils_create(
      AttachCurrentThread(), reinterpret_cast<jlong>(this));
}

ArCoreInstallHelper::~ArCoreInstallHelper() {
  if (!java_install_utils_.is_null()) {
    Java_ArCoreInstallUtils_onNativeDestroy(AttachCurrentThread(),
                                            java_install_utils_);
  }

  RunInstallFinishedCallback(false);
}

void ArCoreInstallHelper::EnsureInstalled(
    int render_process_id,
    int render_frame_id,
    infobars::InfoBarManager* infobar_manager,
    base::OnceCallback<void(bool)> install_callback) {
  DCHECK(!install_finished_callback_);
  install_finished_callback_ = std::move(install_callback);

  if (java_install_utils_.is_null()) {
    RunInstallFinishedCallback(false);
    return;
  }

  // ARCore is not installed or requires an update.
  if (Java_ArCoreInstallUtils_shouldRequestInstallSupportedArCore(
          AttachCurrentThread())) {
    ShowInfoBar(render_process_id, render_frame_id, infobar_manager);
    return;
  }

  // ARCore did not need to be installed/updated so mock out that its
  // installation succeeded.
  OnRequestInstallSupportedArCoreResult(nullptr, true);
}

void ArCoreInstallHelper::ShowInfoBar(
    int render_process_id,
    int render_frame_id,
    infobars::InfoBarManager* infobar_manager) {
  // We can't show an infobar without an |infobar_manager|, so if it's null,
  // report that we are not installed and stop processing.
  if (!infobar_manager) {
    RunInstallFinishedCallback(false);
    return;
  }

  ArCoreAvailability availability = static_cast<ArCoreAvailability>(
      Java_ArCoreInstallUtils_getArCoreInstallStatus(AttachCurrentThread()));
  int message_text = -1;
  int button_text = -1;
  switch (availability) {
    case ArCoreAvailability::kUnsupportedDeviceNotCapable: {
      RunInstallFinishedCallback(false);
      return;  // No need to process further
    }
    case ArCoreAvailability::kUnknownChecking:
    case ArCoreAvailability::kUnknownError:
    case ArCoreAvailability::kUnknownTimedOut:
    case ArCoreAvailability::kSupportedNotInstalled: {
      message_text = IDS_AR_CORE_CHECK_INFOBAR_INSTALL_TEXT;
      button_text = IDS_AR_CORE_CHECK_INFOBAR_INSTALL_BUTTON;
      break;
    }
    case ArCoreAvailability::kSupportedApkTooOld: {
      message_text = IDS_AR_CORE_CHECK_INFOBAR_UPDATE_TEXT;
      button_text = IDS_AR_CORE_CHECK_INFOBAR_UPDATE_BUTTON;
      break;
    }
    case ArCoreAvailability::kSupportedInstalled:
      NOTREACHED();
      break;
  }

  DCHECK_NE(-1, message_text);
  DCHECK_NE(-1, button_text);

  // Binding ourself as a weak ref is okay, since our destructor will still
  // guarantee that the callback is run if we are destroyed while waiting for
  // the callback from the infobar.
  // TODO(ijamardo, https://crbug.com/838833): Add icon for AR info bar.
  auto delegate = std::make_unique<XrInstallInfoBar>(
      infobars::InfoBarDelegate::InfoBarIdentifier::AR_CORE_UPGRADE_ANDROID,
      IDR_ERROR_OUTLINE_GOOGBLUE_24DP, message_text, button_text,
      base::BindOnce(&ArCoreInstallHelper::OnInfoBarResponse,
                     weak_ptr_factory_.GetWeakPtr(), render_process_id,
                     render_frame_id));

  infobar_manager->AddInfoBar(
      infobar_manager->CreateConfirmInfoBar(std::move(delegate)));
}

void ArCoreInstallHelper::OnInfoBarResponse(int render_process_id,
                                            int render_frame_id,
                                            bool try_install) {
  if (!try_install) {
    OnRequestInstallSupportedArCoreResult(nullptr, false);
    return;
  }

  // When completed, java will call: OnRequestInstallSupportedArCoreResult
  Java_ArCoreInstallUtils_requestInstallSupportedArCore(
      AttachCurrentThread(), java_install_utils_,
      GetJavaWebContents(render_process_id, render_frame_id));
}

void ArCoreInstallHelper::OnRequestInstallSupportedArCoreResult(JNIEnv* env,
                                                                bool success) {
  DVLOG(1) << __func__;

  // Nothing else to do, simply call the deferred callback.
  RunInstallFinishedCallback(success);
}

void ArCoreInstallHelper::RunInstallFinishedCallback(bool succeeded) {
  if (install_finished_callback_) {
    std::move(install_finished_callback_).Run(succeeded);
  }
}

static void JNI_ArCoreInstallUtils_InstallArCoreDeviceProviderFactory(
    JNIEnv* env) {
  device::ArCoreDeviceProviderFactory::Install(
      std::make_unique<ArCoreDeviceProviderFactoryImpl>());
}

}  // namespace vr
