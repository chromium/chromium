// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/composebox_query_controller_bridge.h"

#include <utility>

#include "base/android/jni_bytebuffer.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/common/channel_info.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/ComposeBoxQueryControllerBridge_jni.h"

static jlong JNI_ComposeBoxQueryControllerBridge_Init(JNIEnv* env,
                                                      Profile* profile) {
  auto* aim_service = AimEligibilityServiceFactory::GetForProfile(profile);
  if (!aim_service || !aim_service->IsAimEligible()) {
    return 0L;
  }

  ComposeboxQueryControllerBridge* instance =
      new ComposeboxQueryControllerBridge(profile);
  return reinterpret_cast<intptr_t>(instance);
}

ComposeboxQueryControllerBridge::ComposeboxQueryControllerBridge(
    Profile* profile)
    : profile_{profile} {
  auto query_controller_config_params = std::make_unique<
      contextual_search::ContextualSearchContextController::ConfigParams>();
  query_controller_config_params->send_lns_surface = false;
  query_controller_config_params->enable_multi_context_input_flow = false;
  query_controller_config_params->enable_viewport_images = true;
  query_controller_ = std::make_unique<ComposeboxQueryController>(
      IdentityManagerFactory::GetForProfile(profile),
      g_browser_process->shared_url_loader_factory(), chrome::GetChannel(),
      g_browser_process->GetApplicationLocale(),
      TemplateURLServiceFactory::GetForProfile(profile),
      profile->GetVariationsClient(),
      std::move(query_controller_config_params));
  query_controller_->AddObserver(this);
}

ComposeboxQueryControllerBridge::~ComposeboxQueryControllerBridge() = default;

void ComposeboxQueryControllerBridge::Destroy(JNIEnv* env) {
  query_controller_->RemoveObserver(this);
  delete this;
}

void ComposeboxQueryControllerBridge::NotifySessionStarted(JNIEnv* env) {
  query_controller_->InitializeIfNeeded();
}

void ComposeboxQueryControllerBridge::NotifySessionAbandoned(JNIEnv* env) {
  // No-op.
}

base::android::ScopedJavaLocalRef<jobject>
ComposeboxQueryControllerBridge::AddFile(
    JNIEnv* env,
    std::string& file_name,
    std::string& file_type,
    const jni_zero::JavaParamRef<jobject>& file_data) {
  base::UnguessableToken file_token = base::UnguessableToken::Create();

  std::optional<lens::ImageEncodingOptions> image_options = std::nullopt;
  lens::MimeType mime_type;
  AimEligibilityService* aim_service =
      AimEligibilityServiceFactory::GetForProfile(profile_);

  if (file_type.find("pdf") != std::string::npos) {
    if (!aim_service->IsPdfUploadEligible()) {
      return {};
    }

    mime_type = lens::MimeType::kPdf;
  } else if (file_type.find("image") != std::string::npos) {
    mime_type = lens::MimeType::kImage;
    image_options = lens::ImageEncodingOptions{.enable_webp_encoding = false,
                                               .max_size = 1500000,
                                               .max_height = 1600,
                                               .max_width = 1600,
                                               .compression_quality = 40};
  } else {
    // Unsupported mime type.
    return {};
  }

  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->primary_content_type = mime_type;

  base::span<const uint8_t> file_bytes_span =
      base::android::JavaByteBufferToSpan(env, file_data);
  std::vector<uint8_t> file_data_vector(file_bytes_span.begin(),
                                        file_bytes_span.end());
  input_data->context_input->push_back(
      lens::ContextualInput(std::move(file_data_vector), mime_type));
  query_controller_->StartFileUploadFlow(file_token, std::move(input_data),
                                         std::move(image_options));

  return base::android::ConvertUTF8ToJavaString(env, file_token.ToString());
}

base::android::ScopedJavaLocalRef<jobject>
ComposeboxQueryControllerBridge::AddTabContext(
    JNIEnv* env,
    content::WebContents* web_contents) {
  tabs::TabInterface* const tab =
      tabs::TabInterface::GetFromContents(web_contents);

  if (!tab) {
    return {};
  }

  base::UnguessableToken file_token = base::UnguessableToken::Create();
  lens::TabContextualizationController* tab_contextualization_controller =
      lens::TabContextualizationController::From(tab);
  if (!tab_contextualization_controller) {
    return {};
  }

  tab_contextualization_controller->GetPageContext(
      base::BindOnce(&ComposeboxQueryControllerBridge::OnGetTabPageContext,
                     weak_ptr_factory_.GetWeakPtr(), env, file_token));

  return base::android::ConvertUTF8ToJavaString(env, file_token.ToString());
}

GURL ComposeboxQueryControllerBridge::GetAimUrl(JNIEnv* env,
                                                std::string& query_text) {
  // TODO(crbug.com/448149357): Update the bridge interface to take in
  // additional params for the create search url request info.
  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
      search_url_request_info = std::make_unique<
          ComposeboxQueryController::CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = query_text;
  search_url_request_info->query_start_time = base::Time::Now();
  return query_controller_->CreateSearchUrl(std::move(search_url_request_info));
}

void ComposeboxQueryControllerBridge::RemoveAttachment(
    JNIEnv* env,
    const std::string& token) {
  std::optional<base::UnguessableToken> unguessable_token =
      base::UnguessableToken::DeserializeFromString(token);
  if (unguessable_token.has_value()) {
    query_controller_->DeleteFile(unguessable_token.value());
  }
}

void ComposeboxQueryControllerBridge::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    contextual_search::FileUploadStatus file_upload_status,
    const std::optional<contextual_search::FileUploadErrorType>& error_type) {}

void ComposeboxQueryControllerBridge::OnGetTabPageContext(
    JNIEnv* env,
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  std::optional<lens::ImageEncodingOptions> image_options =
      lens::ImageEncodingOptions{.enable_webp_encoding = false,
                                 .max_size = 1500000,
                                 .max_height = 1600,
                                 .max_width = 1600,
                                 .compression_quality = 40};

  query_controller_->StartFileUploadFlow(
      context_token, std::move(page_content_data), std::move(image_options));
}
