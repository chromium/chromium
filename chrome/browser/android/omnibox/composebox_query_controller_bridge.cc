// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/composebox_query_controller_bridge.h"

#include "base/android/jni_bytebuffer.h"
#include "base/containers/span.h"
#include "base/unguessable_token.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/android/omnibox/jni_headers/ComposeBoxQueryControllerBridge_jni.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/common/channel_info.h"

static jlong JNI_ComposeBoxQueryControllerBridge_Init(JNIEnv* env,
                                                      Profile* profile) {
  ComposeboxQueryControllerBridge* instance =
      new ComposeboxQueryControllerBridge(profile);
  return reinterpret_cast<intptr_t>(instance);
}

ComposeboxQueryControllerBridge::ComposeboxQueryControllerBridge(
    Profile* profile) {
  query_controller_ = std::make_unique<ComposeboxQueryController>(
      IdentityManagerFactory::GetForProfile(profile),
      g_browser_process->shared_url_loader_factory(), chrome::GetChannel(),
      g_browser_process->GetApplicationLocale(),
      TemplateURLServiceFactory::GetForProfile(profile),
      profile->GetVariationsClient(), false);
  query_controller_->AddObserver(this);
}

ComposeboxQueryControllerBridge::~ComposeboxQueryControllerBridge() = default;

void ComposeboxQueryControllerBridge::Destroy(JNIEnv* env) {
  query_controller_->RemoveObserver(this);
  delete this;
}

void ComposeboxQueryControllerBridge::NotifySessionStarted(JNIEnv* env) {
  query_controller_->NotifySessionStarted();
}

void ComposeboxQueryControllerBridge::NotifySessionAbandoned(JNIEnv* env) {
  query_controller_->NotifySessionAbandoned();
}

void ComposeboxQueryControllerBridge::AddFile(
    JNIEnv* env,
    std::string& file_name,
    std::string& file_type,
    const jni_zero::JavaParamRef<jobject>& file_data) {
  base::span<const uint8_t> file_bytes_span =
      base::android::JavaByteBufferToSpan(env, file_data);
  scoped_refptr<base::RefCountedBytes> file_bytes =
      base::MakeRefCounted<base::RefCountedBytes>(file_bytes_span);

  auto file_info_metadata =
      std::make_unique<ComposeboxQueryController::FileInfo>();
  file_info_metadata->file_name = file_name;
  file_info_metadata->file_size_bytes = file_bytes_span.size();
  file_info_metadata->file_token_ = base::UnguessableToken::Create();

  std::optional<composebox::ImageEncodingOptions> image_options = std::nullopt;

  if (file_type.find("pdf") != std::string::npos) {
    file_info_metadata->mime_type_ = lens::MimeType::kPdf;
  } else if (file_type.find("image") != std::string::npos) {
    file_info_metadata->mime_type_ = lens::MimeType::kImage;
    image_options =
        composebox::ImageEncodingOptions{.enable_webp_encoding = false,
                                         .max_size = 1500000,
                                         .max_height = 1600,
                                         .max_width = 1600,
                                         .compression_quality = 40};
  } else {
    NOTREACHED();
  }

  if ((file_type).find("pdf") != std::string::npos) {
    file_info_metadata->mime_type_ = lens::MimeType::kPdf;
  } else if ((file_type).find("image") != std::string::npos) {
    file_info_metadata->mime_type_ = lens::MimeType::kImage;
  }

  query_controller_->StartFileUploadFlow(std::move(file_info_metadata),
                                         std::move(file_bytes),
                                         std::move(image_options));
}

void ComposeboxQueryControllerBridge::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    composebox_query::mojom::FileUploadStatus file_upload_status,
    const std::optional<FileUploadErrorType>& error_type) {}
