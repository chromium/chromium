// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/send_tab_to_self_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/share/android/jni_headers/SendTabToSelfInfoBar_jni.h"
#include "components/infobars/android/infobar_android.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"

namespace send_tab_to_self {

SendTabToSelfInfoBar::SendTabToSelfInfoBar(
    std::unique_ptr<SendTabToSelfInfoBarDelegate> delegate)
    : infobars::InfoBarAndroid(
          std::move(delegate),
          base::BindRepeating(&ResourceMapper::MapToJavaDrawableId)) {}

SendTabToSelfInfoBar::~SendTabToSelfInfoBar() = default;

void SendTabToSelfInfoBar::ProcessButton(int action) {
  NOTREACHED();  // No button on this infobar.
}

base::android::ScopedJavaLocalRef<jobject>
SendTabToSelfInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  return Java_SendTabToSelfInfoBar_create(env);
}

void SendTabToSelfInfoBar::OnLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  // TODO(crbug.com/949233): Open the tab here via the delegate
  NOTIMPLEMENTED();
}

// static
void SendTabToSelfInfoBar::ShowInfoBar(
    content::WebContents* web_contents,
    std::unique_ptr<SendTabToSelfInfoBarDelegate> delegate) {
  InfoBarService* service = InfoBarService::FromWebContents(web_contents);
  service->AddInfoBar(
      base::WrapUnique(new SendTabToSelfInfoBar(std::move(delegate))));
}

}  // namespace send_tab_to_self
