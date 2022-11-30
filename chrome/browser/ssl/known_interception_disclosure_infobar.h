// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_H_
#define CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/infobars/core/infobar.h"

class KnownInterceptionDisclosureInfoBarDelegate;

// KnownInterceptionDisclosureInfoBar is a thin veneer over ConfirmInfoBar that
// adds a discrete description (instead of just having a title).
class KnownInterceptionDisclosureInfoBar : public infobars::ConfirmInfoBar {
 public:
  static std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<KnownInterceptionDisclosureInfoBarDelegate> delegate);
  ~KnownInterceptionDisclosureInfoBar() override = default;

  KnownInterceptionDisclosureInfoBar(
      const KnownInterceptionDisclosureInfoBar&) = delete;
  KnownInterceptionDisclosureInfoBar& operator=(
      const KnownInterceptionDisclosureInfoBar&) = delete;

 private:
  explicit KnownInterceptionDisclosureInfoBar(
      std::unique_ptr<KnownInterceptionDisclosureInfoBarDelegate> delegate);

  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  KnownInterceptionDisclosureInfoBarDelegate* GetDelegate();
};

#endif  // CHROME_BROWSER_SSL_KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_H_
