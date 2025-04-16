// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/enterprise/util/jni_headers/DataProtectionBridge_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserContext;
using content::ClipboardEndpoint;
using content::ClipboardPasteData;
using content::GlobalRenderFrameHostId;
using content::RenderFrameHost;

// TODO(crbug.com/410835513): Unify with other declarations of
// CreateDataEndpoint
std::unique_ptr<ui::DataTransferEndpoint> CreateDataEndpoint(
    RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetMainFrame()->GetLastCommittedURL().is_valid()) {
    return nullptr;
  }
  return std::make_unique<ui::DataTransferEndpoint>(
      render_frame_host->GetMainFrame()->GetLastCommittedURL(),
      ui::DataTransferEndpointOptions{
          .notify_if_restricted =
              render_frame_host->HasTransientUserActivation(),
          .off_the_record =
              render_frame_host->GetBrowserContext()->IsOffTheRecord(),
      });
}

// TODO(crbug.com/410835513): Unify with other declarations of
// CreateClipboardEndpoint
ClipboardEndpoint CreateClipboardEndpoint(RenderFrameHost* render_frame_host) {
  return ClipboardEndpoint(
      CreateDataEndpoint(render_frame_host).get(),
      base::BindRepeating(
          [](GlobalRenderFrameHostId rfh_id) -> BrowserContext* {
            auto* rfh = RenderFrameHost::FromID(rfh_id);
            if (!rfh) {
              return nullptr;
            }
            return rfh->GetBrowserContext();
          },
          render_frame_host->GetGlobalId()),
      *render_frame_host);
}

// TODO(crbug.com/387484337) Add instrumentation tests
void JNI_DataProtectionBridge_VerifyCopyIsAllowedByPolicy(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_text,
    const base::android::JavaParamRef<jobject>& jrender_frame_host,
    const JavaParamRef<jobject>& j_callback) {
  std::u16string text = base::android::ConvertJavaStringToUTF16(env, j_text);
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);

  ClipboardPasteData data;
  data.text = text;

  base::OnceCallback<void(bool)> boolean_java_callback =
      base::BindOnce(&base::android::RunBooleanCallbackAndroid,
                     ScopedJavaGlobalRef<jobject>(j_callback));

  enterprise_data_protection::IsClipboardCopyAllowedByPolicy(
      CreateClipboardEndpoint(render_frame_host),
      {
          .size = text.size() * sizeof(std::u16string::value_type),
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      data,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             const ui::ClipboardFormatType& type,
             const ClipboardPasteData& data,
             std::optional<std::u16string> replacement_data) {
            std::move(callback).Run(!data.empty());
          },
          std::move(boolean_java_callback)));
}
