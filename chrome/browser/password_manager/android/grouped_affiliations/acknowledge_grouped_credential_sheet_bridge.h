// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_BRIDGE_H_

#include <jni.h>

#include "base/functional/callback_helpers.h"
#include "base/types/pass_key.h"
#include "ui/gfx/native_widget_types.h"

// JNI bridge to display the acknowledgement sheet when filling grouped
// credentials on Android.
class AcknowledgeGroupedCredentialSheetBridge {
 public:
  class JniDelegate {
   public:
    JniDelegate();
    JniDelegate(const JniDelegate&) = delete;
    JniDelegate& operator=(const JniDelegate&) = delete;
    virtual ~JniDelegate() = 0;

    virtual void Create(const gfx::NativeWindow window_android,
                        AcknowledgeGroupedCredentialSheetBridge* bridge) = 0;
    virtual void Show(std::string current_origin,
                      std::string credential_origin) = 0;
    virtual void Dismiss() = 0;
  };
  AcknowledgeGroupedCredentialSheetBridge();
  // Test constructors
  AcknowledgeGroupedCredentialSheetBridge(
      base::PassKey<
          class AcknowledgeGroupedCredentialSheetControllerTestHelper>,
      std::unique_ptr<JniDelegate> jni_delegate);
  AcknowledgeGroupedCredentialSheetBridge(
      base::PassKey<class AcknowledgeGroupedCredentialSheetControllerTest>,
      std::unique_ptr<JniDelegate> jni_delegate);

  AcknowledgeGroupedCredentialSheetBridge(
      const AcknowledgeGroupedCredentialSheetBridge&) = delete;
  AcknowledgeGroupedCredentialSheetBridge& operator=(
      const AcknowledgeGroupedCredentialSheetBridge&) = delete;

  ~AcknowledgeGroupedCredentialSheetBridge();

  void Show(std::string current_origin,
            std::string credential_origin,
            gfx::NativeWindow window,
            base::OnceCallback<void(bool)> closure_callback);
  void OnDismissed(JNIEnv* env, bool accepted);

 private:
  base::OnceCallback<void(bool)> closure_callback_;
  std::unique_ptr<JniDelegate> jni_delegate_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_BRIDGE_H_
