// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/composebox_query_controller_bridge.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_bytebuffer.h"
#include "base/base64.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/common/channel_info.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/lens_url_utils.h"
#include "components/lens/tab_contextualization_controller.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_cache.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/ComposeBoxQueryControllerBridge_jni.h"

namespace {
void RunJavaCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
    GURL url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      j_callback, url::GURLAndroid::FromNativeGURL(env, url));
}
}  // namespace

static jlong JNI_ComposeBoxQueryControllerBridge_Init(
    JNIEnv* env,
    Profile* profile,
    const base::android::JavaParamRef<jobject>& java_obj) {
  auto* aim_service = AimEligibilityServiceFactory::GetForProfile(profile);
  if (!aim_service || !aim_service->IsAimEligible()) {
    return 0L;
  }

  // TODO(crbug.com/469142288): this should only disable sharing; for now the
  // resolution is that in M144 we disable all of the fusebox.
  if (!contextual_search::ContextualSearchService::IsContextSharingEnabled(
          profile->GetPrefs())) {
    return 0L;
  }

  ComposeboxQueryControllerBridge* instance =
      new ComposeboxQueryControllerBridge(profile, java_obj);
  return reinterpret_cast<intptr_t>(instance);
}

ComposeboxQueryControllerBridge::ComposeboxQueryControllerBridge(
    Profile* profile,
    const base::android::JavaParamRef<jobject>& java_obj)
    : profile_{profile}, java_obj_(java_obj) {
  auto query_controller_config_params = std::make_unique<
      contextual_search::ContextualSearchContextController::ConfigParams>();
  query_controller_config_params->send_lns_surface = false;
  query_controller_config_params->enable_multi_context_input_flow =
      OmniboxFieldTrial::kOmniboxMultimodalInputMultiContext.Get();
  query_controller_config_params->enable_viewport_images = true;
  query_controller_config_params
      ->use_separate_request_ids_for_multi_context_viewport_images = false;
  query_controller_config_params
      ->prioritize_suggestions_for_the_first_attached_document =
      OmniboxFieldTrial::kOmniboxMultimodalPrioritizeSuggestionsForFirstDocument
          .Get();

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

base::WeakPtr<ComposeboxQueryControllerBridge>
ComposeboxQueryControllerBridge::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

base::android::ScopedJavaLocalRef<jobject>
ComposeboxQueryControllerBridge::AddTabContextFromCache(JNIEnv* env,
                                                        long tab_id) {
  page_content_annotations::PageContentExtractionService* service =
      page_content_annotations::PageContentExtractionServiceFactory::
          GetForProfile(profile_);
  if (!service) {
    return {};
  }

  page_content_annotations::PageContentCache* cache =
      service->GetPageContentCache();
  if (!cache) {
    return {};
  }

  base::UnguessableToken file_token = base::UnguessableToken::Create();

  cache->GetPageContentForTab(
      tab_id, base::BindOnce(
                  &ComposeboxQueryControllerBridge::OnGetPageContentFromCache,
                  weak_ptr_factory_.GetWeakPtr(), env, file_token));

  return base::android::ConvertUTF8ToJavaString(env, file_token.ToString());
}

std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
ComposeboxQueryControllerBridge::CreateSearchUrlRequestInfoFromUrl(GURL url) {
  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
      search_url_request_info = std::make_unique<
          ComposeboxQueryController::CreateSearchUrlRequestInfo>();
  net::GetValueForKeyInQuery(url, "q", &search_url_request_info->query_text);
  search_url_request_info->additional_params =
      lens::GetParametersMapWithoutQuery(url);
  search_url_request_info->query_start_time = base::Time::Now();
  // Read the list of tokens from the fileinfo map in the contextual search
  // controller.
  // TODO(crbug.com/455952553): Rely on the contextual search session handle
  // to track uploaded context tokens.
  for (const contextual_search::FileInfo* file_info :
       query_controller_->GetFileInfoList()) {
    search_url_request_info->file_tokens.push_back(file_info->file_token);
  }
  return search_url_request_info;
}

void ComposeboxQueryControllerBridge::GetAimUrl(
    JNIEnv* env,
    GURL url,
    const base::android::JavaRef<jobject>& j_callback) {
  auto search_url_request_info =
      CreateSearchUrlRequestInfoFromUrl(std::move(url));
  query_controller_->CreateSearchUrl(
      std::move(search_url_request_info),
      base::BindOnce(&RunJavaCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ComposeboxQueryControllerBridge::GetImageGenerationUrl(
    JNIEnv* env,
    GURL url,
    const base::android::JavaRef<jobject>& j_callback) {
  auto search_url_request_info =
      CreateSearchUrlRequestInfoFromUrl(std::move(url));
  search_url_request_info->additional_params["imgn"] = "1";
  query_controller_->CreateSearchUrl(
      std::move(search_url_request_info),
      base::BindOnce(&RunJavaCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
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

bool ComposeboxQueryControllerBridge::IsPdfUploadEligible(JNIEnv* env) {
  AimEligibilityService* aim_service =
      AimEligibilityServiceFactory::GetForProfile(profile_);
  return aim_service && aim_service->IsPdfUploadEligible();
}

bool ComposeboxQueryControllerBridge::IsCreateImagesEligible(JNIEnv* env) {
  AimEligibilityService* aim_service =
      AimEligibilityServiceFactory::GetForProfile(profile_);
  return aim_service && aim_service->IsCreateImagesEligible();
}

std::unique_ptr<lens::proto::LensOverlaySuggestInputs>
ComposeboxQueryControllerBridge::CreateLensOverlaySuggestInputs() const {
  auto tokens = std::vector<base::UnguessableToken>();
  // Read the list of tokens from the fileinfo list in the contextual search
  // controller.
  // TODO(crbug.com/455843962): Rely on the contextual search session handle
  // to track uploaded context tokens.
  for (const contextual_search::FileInfo* file_info :
       query_controller_->GetFileInfoList()) {
    tokens.push_back(file_info->file_token);
  }
  return query_controller_->CreateSuggestInputs(tokens);
}

void ComposeboxQueryControllerBridge::OnFileUploadStatusChanged(
    const base::UnguessableToken& file_token,
    lens::MimeType mime_type,
    contextual_search::FileUploadStatus file_upload_status,
    const std::optional<contextual_search::FileUploadErrorType>& error_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ComposeBoxQueryControllerBridge_onFileUploadStatusChanged(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env, file_token.ToString()),
      static_cast<int>(file_upload_status));
}

void ComposeboxQueryControllerBridge::OnGetTabPageContext(
    JNIEnv* env,
    const base::UnguessableToken& context_token,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (!page_content_data->context_input.has_value() ||
      page_content_data->context_input->size() <= 0) {
    OnFileUploadStatusChanged(
        context_token, lens::MimeType::kUnknown,
        contextual_search::FileUploadStatus::kValidationFailed,
        contextual_search::FileUploadErrorType::kBrowserProcessingError);
    return;
  }

  std::optional<lens::ImageEncodingOptions> image_options =
      lens::ImageEncodingOptions{.enable_webp_encoding = false,
                                 .max_size = 1500000,
                                 .max_height = 1600,
                                 .max_width = 1600,
                                 .compression_quality = 40};

  query_controller_->StartFileUploadFlow(
      context_token, std::move(page_content_data), std::move(image_options));
}

void ComposeboxQueryControllerBridge::OnGetPageContentFromCache(
    JNIEnv* env,
    const base::UnguessableToken& context_token,
    std::optional<optimization_guide::proto::PageContext> page_context) {
  // TODO(crbug.com/457869241): Merge this and the code in
  // TabContextualizationController.
  if (!page_context.has_value()) {
    OnFileUploadStatusChanged(
        context_token, lens::MimeType::kUnknown,
        contextual_search::FileUploadStatus::kValidationFailed,
        contextual_search::FileUploadErrorType::kBrowserProcessingError);
    return;
  }

  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->primary_content_type = lens::MimeType::kAnnotatedPageContent;

  // Page URL and Title.
  if (page_context->has_url()) {
    input_data->page_url = GURL(page_context->url());
  }
  if (page_context->has_title()) {
    input_data->page_title = page_context->title();
  }

#if BUILDFLAG(ENABLE_PDF)
  // TODO(crbug.com/457869538): Handle pdf.
#endif  // BUILDFLAG(ENABLE_PDF)

  // If the page is not a PDF, get the annotated page content.
  if (page_context->has_annotated_page_content()) {
    std::string serialized_apc;
    page_context->annotated_page_content().SerializeToString(&serialized_apc);
    input_data->context_input->emplace_back(
        std::vector<uint8_t>(serialized_apc.begin(), serialized_apc.end()),
        lens::MimeType::kAnnotatedPageContent);
  }

  if (page_content_annotations::features::kPageContentCacheEnableScreenshot
          .Get() &&
      page_context->has_tab_screenshot()) {
    const std::string& base64_string = page_context->tab_screenshot();
    std::string png_data_string;

    if (base::Base64Decode(base64_string, &png_data_string)) {
      input_data->viewport_screenshot_bytes =
          std::vector<uint8_t>(png_data_string.begin(), png_data_string.end());
    }
  }

  OnGetTabPageContext(env, context_token, std::move(input_data));
}

DEFINE_JNI(ComposeBoxQueryControllerBridge)
