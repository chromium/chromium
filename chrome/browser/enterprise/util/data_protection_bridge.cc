// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_features.h"
#include "chrome/browser/enterprise/data_controls/chrome_rules_service.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metadata.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/enterprise/util/jni_headers/DataProtectionBridge_jni.h"

using content::ClipboardPasteData;
using content::RenderFrameHost;

namespace {

void VerifyCopyIsAllowedByPolicy(RenderFrameHost* render_frame_host,
                                 base::OnceCallback<void(bool)> callback,
                                 const ui::ClipboardMetadata& metadata,
                                 const content::ClipboardPasteData& data) {
  if (!render_frame_host) {
    std::move(callback).Run(true);
    return;
  }

  enterprise_data_protection::IsClipboardCopyAllowedByPolicy(
      content::CreateClipboardEndpoint(*render_frame_host), metadata, data,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             const ui::ClipboardFormatType& type,
             const ClipboardPasteData& data,
             std::optional<std::u16string> replacement_data) {
            std::move(callback).Run(!data.empty());
          },
          std::move(callback)));
}

void VerifyShareIsAllowedByPolicy(RenderFrameHost* render_frame_host,
                                  base::OnceCallback<void(bool)> callback,
                                  const ui::ClipboardMetadata& metadata,
                                  const content::ClipboardPasteData& data) {
  if (!render_frame_host) {
    std::move(callback).Run(true);
    return;
  }

  enterprise_data_protection::IsClipboardShareAllowedByPolicy(
      content::CreateClipboardEndpoint(*render_frame_host), metadata, data,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             const ui::ClipboardFormatType& type,
             const ClipboardPasteData& data,
             std::optional<std::u16string> replacement_data) {
            std::move(callback).Run(!data.empty());
          },
          std::move(callback)));
}

void VerifyGenericCopyActionIsAllowedByPolicy(
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback,
    const ui::ClipboardMetadata& metadata,
    const content::ClipboardPasteData& data) {
  if (!render_frame_host) {
    std::move(callback).Run(true);
    return;
  }

  enterprise_data_protection::IsClipboardGenericCopyActionAllowedByPolicy(
      content::CreateClipboardEndpoint(*render_frame_host), metadata, data,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             const ui::ClipboardFormatType& type,
             const ClipboardPasteData& data,
             std::optional<std::u16string> replacement_data) {
            std::move(callback).Run(!data.empty());
          },
          std::move(callback)));
}

}  // namespace

static bool JNI_DataProtectionBridge_HasBlockingScreenshotRule(
    Profile* profile) {
  if (!profile) {
    return false;
  }

  data_controls::ChromeRulesService* rules_service =
      data_controls::ChromeRulesServiceFactory::GetInstance()
          ->GetForBrowserContext(profile);
  if (!rules_service) {
    return false;
  }

  return rules_service->HasBlockingScreenshotRule();
}

static bool JNI_DataProtectionBridge_IsScreenshotAllowed(TabAndroid* tab) {
  if (!tab) {
    return true;
  }

  enterprise_data_protection::DataProtectionNavigationController* controller =
      tab->GetTabFeatures()->data_protection_controller();
  if (!controller) {
    return true;
  }

  return controller->screenshot_allowed();
}

static void JNI_DataProtectionBridge_RegisterScreenshotSubscriptionCallback(
    TabAndroid* tab,
    base::RepeatingCallback<void(bool)> callback) {
  if (!tab) {
    return;
  }
  enterprise_data_protection::DataProtectionNavigationController* controller =
      tab->GetTabFeatures()->data_protection_controller();
  if (!controller) {
    return;
  }
  controller->current_callback_subscription_ =
      controller->RegisterScreenshotAllowedUpdatedCallback(callback);
}

static void JNI_DataProtectionBridge_ClearScreenshotSubscriptionCallback(
    TabAndroid* tab) {
  if (!tab) {
    return;
  }
  enterprise_data_protection::DataProtectionNavigationController* controller =
      tab->GetTabFeatures()->data_protection_controller();

  if (!controller) {
    return;
  }
  controller->current_callback_subscription_ = base::CallbackListSubscription();
}

// TODO(crbug.com/387484337) Add instrumentation tests
static void JNI_DataProtectionBridge_VerifyCopyTextIsAllowedByPolicy(
    const std::u16string& text,
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  ClipboardPasteData data;
  data.text = text;

  VerifyCopyIsAllowedByPolicy(
      render_frame_host, std::move(callback),
      {
          .size = text.size() * sizeof(std::u16string::value_type),
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      data);
}

// TODO(crbug.com/387484337) Add instrumentation tests
static void JNI_DataProtectionBridge_VerifyCopyUrlIsAllowedByPolicy(
    const std::u16string& url,
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  ClipboardPasteData data;
  data.text = url;

  VerifyCopyIsAllowedByPolicy(
      render_frame_host, std::move(callback),
      {
          .size = url.size() * sizeof(std::u16string::value_type),
          .format_type = ui::ClipboardFormatType::UrlType(),
      },
      data);
}

// TODO(crbug.com/387484337) Add instrumentation tests
static void JNI_DataProtectionBridge_VerifyCopyImageIsAllowedByPolicy(
    const std::u16string& image_uri,
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  ClipboardPasteData data;
  data.text = image_uri;

  VerifyCopyIsAllowedByPolicy(
      render_frame_host, std::move(callback),
      {
          // TODO(crbug.com/344593255): Retrieve the bitmap size when it's
          //  needed by the data controls logic.
          .format_type = ui::ClipboardFormatType::BitmapType(),
      },
      data);
}

// TODO(crbug.com/387484337) Add instrumentation tests
static void JNI_DataProtectionBridge_VerifyShareTextIsAllowedByPolicy(
    const std::u16string& text,
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  ClipboardPasteData data;
  data.text = text;

  VerifyShareIsAllowedByPolicy(
      render_frame_host, std::move(callback),
      {
          .size = text.size() * sizeof(std::u16string::value_type),
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      data);
}

// TODO(crbug.com/387484337) Add instrumentation tests
static void JNI_DataProtectionBridge_VerifyShareUrlIsAllowedByPolicy(
    const std::u16string& url,
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  ClipboardPasteData data;
  data.text = url;

  VerifyShareIsAllowedByPolicy(
      render_frame_host, std::move(callback),
      {
          .size = url.size() * sizeof(std::u16string::value_type),
          .format_type = ui::ClipboardFormatType::UrlType(),
      },
      data);
}

// TODO(crbug.com/387484337) Add instrumentation tests
static void JNI_DataProtectionBridge_VerifyShareImageIsAllowedByPolicy(
    const std::u16string& image_uri,
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  ClipboardPasteData data;
  data.text = image_uri;

  VerifyShareIsAllowedByPolicy(
      render_frame_host, std::move(callback),
      {
          // TODO(crbug.com/344593255): Retrieve the bitmap size when it's
          //  needed by the data controls logic.
          .format_type = ui::ClipboardFormatType::BitmapType(),
      },
      data);
}

// TODO(crbug.com/387484337) Add instrumentation tests
static void
JNI_DataProtectionBridge_VerifyGenericCopyImageActionIsAllowedByPolicy(
    const std::u16string& image_uri,
    RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  ClipboardPasteData data;
  data.text = image_uri;

  VerifyGenericCopyActionIsAllowedByPolicy(
      render_frame_host, std::move(callback),
      {
          // TODO(crbug.com/344593255): Retrieve the bitmap size when it's
          //  needed by the data controls logic.
          .format_type = ui::ClipboardFormatType::BitmapType(),
      },
      data);
}

static bool JNI_DataProtectionBridge_IsSearchWithAllowed(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return true;
  }

  return enterprise_data_protection::IsSearchWithAllowed(web_contents);
}

static void JNI_DataProtectionBridge_ShouldAllowSearchWith(
    int32_t text_length,
    content::WebContents* web_contents,
    base::OnceClosure callback) {
  if (!web_contents) {
    std::move(callback).Run();
    return;
  }

  enterprise_data_protection::ShouldAllowSearchWith(
      web_contents, text_length * sizeof(std::u16string::value_type),
      std::move(callback));
}

DEFINE_JNI(DataProtectionBridge)
