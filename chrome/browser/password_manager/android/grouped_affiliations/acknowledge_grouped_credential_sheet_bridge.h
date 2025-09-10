// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_BRIDGE_H_

#include <jni.h>

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/types/pass_key.h"
#include "ui/gfx/native_ui_types.h"

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
    virtual void Show(const std::string& current_hostname,
                      const std::string& credential_hostname) = 0;
    virtual void Dismiss() = 0;
  };
  // Represents all possible ways to resolve the sheet: acceptance, clicking
  // `Back`, or just dismissing it. The difference between `Back` and just
  // dismissing is that the first may navigate user to the previous UI (like
  // TTF). These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //    org.chromium.chrome.browser.grouped_affiliations)
  enum class DismissReason {
    kAccept = 0,
    kBack = 1,
    kIgnore = 2,
    kMaxValue = kIgnore,
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

  void Show(const std::string& current_hostname,
            const std::string& credential_hostname,
            gfx::NativeWindow window,
            base::OnceCallback<void(DismissReason)> closure_callback);
  void OnDismissed(JNIEnv* env, int dismiss_reason);

 private:
  base::OnceCallback<void(DismissReason)> closure_callback_;
  std::unique_ptr<JniDelegate> jni_delegate_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_BRIDGE_H_
