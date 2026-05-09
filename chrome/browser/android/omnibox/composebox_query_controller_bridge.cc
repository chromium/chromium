// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/composebox_query_controller_bridge.h"

#include <memory>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/android/omnibox/tab_context_capture_request.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/channel_info.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/internal/composebox_query_controller.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_url_utils.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_cache.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/url_util.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "third_party/omnibox_proto/tool_config.pb.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/ComposeboxQueryControllerBridge_jni.h"
#include "chrome/browser/ui/android/omnibox/jni_headers/SuggestedTabInfo_jni.h"
#include "components/contextual_search/jni_headers/InputState_jni.h"

namespace {
void RunJavaCallback(
    const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
    GURL url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      j_callback, url::GURLAndroid::FromNativeGURL(env, url));
}
}  // namespace

static int64_t JNI_ComposeboxQueryControllerBridge_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_obj,
    Profile* profile,
    content::WebContents* contextual_tasks_web_contents) {
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
      new ComposeboxQueryControllerBridge(java_obj, profile,
                                          contextual_tasks_web_contents);
  return reinterpret_cast<intptr_t>(instance);
}

ComposeboxQueryControllerBridge::ComposeboxQueryControllerBridge(
    const base::android::JavaRef<jobject>& java_obj,
    Profile* profile,
    content::WebContents* contextual_tasks_web_contents)
    : profile_{profile}, java_obj_(java_obj) {
  is_task_scoped_ =
      contextual_tasks_web_contents != nullptr &&
      base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks);
  if (is_task_scoped_ && contextual_tasks_web_contents &&
      !contextual_tasks_web_contents->IsBeingDestroyed()) {
    contextual_tasks_web_ui_interface_ =
        contextual_tasks::GetWebUiInterface(contextual_tasks_web_contents);
    if (contextual_tasks_web_ui_interface_) {
      contextual_tasks_web_ui_interface_->SetComposeboxHandler(this);
    }
  }

  auto query_controller_config_params = std::make_unique<
      contextual_search::ContextualSearchContextController::ConfigParams>();
  query_controller_config_params->send_lns_surface = false;
  query_controller_config_params->enable_viewport_images = true;
  query_controller_config_params
      ->prioritize_suggestions_for_the_first_attached_document =
      OmniboxFieldTrial::kOmniboxMultimodalPrioritizeSuggestionsForFirstDocument
          .Get();

  contextual_search::ContextualSearchService* search_service =
      ContextualSearchServiceFactory::GetForProfile(profile);
  session_handle_ = search_service->CreateSession(
      std::move(query_controller_config_params),
      contextual_search::ContextualSearchSource::kOmnibox,
      lens::LensOverlayInvocationSource::kOmniboxContextualQuery);

  if (!session_handle_->CheckSearchContentSharingSettings(
          profile->GetPrefs())) {
    // TODO(https://crbug.com/470404040): Handle should support a broken state
    // where the service is null and calls are no-oped. Otherwise we allow
    // future calls to fail when things already should be disabled.
    return;
  }

  InitializeInputStateModel();

  contextual_tasks::ContextualTasksService* tasks_service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile);
  query_contextualizer_ =
      std::make_unique<contextual_tasks::QueryContextualizer>(tasks_service,
                                                              this);

  query_controller()->AddObserver(this);
}

ComposeboxQueryControllerBridge::~ComposeboxQueryControllerBridge() = default;

void ComposeboxQueryControllerBridge::Destroy(JNIEnv* env) {
  // Query controller is accessed through a weak ptr, possible that during
  // shutdown it's already gone.
  contextual_search::ContextualSearchContextController* controller =
      query_controller();
  if (controller) {
    controller->RemoveObserver(this);
  }

  delete this;
}

void ComposeboxQueryControllerBridge::OnWebUIDestroyed(JNIEnv* env) {
  contextual_tasks_web_ui_interface_ = nullptr;
}

size_t ComposeboxQueryControllerBridge::GetAttachmentCount() const {
  // Uploaded context tokens are updated as soon as
  // session_handle_->CreateContextToken() is called in the file or tab context
  // upload flows.
  return session_handle_->GetUploadedContextTokens().size();
}

base::WeakPtr<ComposeboxQueryControllerBridge>
ComposeboxQueryControllerBridge::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

GURL ComposeboxQueryControllerBridge::GetTabUrl(
    contextual_tasks::QueryContextualizer::TabId id) {
  // No-op.
  return GURL();
}

SessionID ComposeboxQueryControllerBridge::GetTabSessionId(
    contextual_tasks::QueryContextualizer::TabId id) {
  // No-op.
  return SessionID::InvalidValue();
}

void ComposeboxQueryControllerBridge::GetPageContext(
    contextual_tasks::QueryContextualizer::TabId id,
    base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
        callback) {
  // No-op.
  std::move(callback).Run(nullptr);
}

bool ComposeboxQueryControllerBridge::IsTabValid(
    contextual_tasks::QueryContextualizer::TabId id) {
  // No-op.
  return false;
}

std::optional<lens::ImageEncodingOptions> ComposeboxQueryControllerBridge::
    GetTabViewportEncodingOptionsForQueryContextualizer() {
  // No-op.
  return std::nullopt;
}

contextual_search::ContextualSearchSessionHandle*
ComposeboxQueryControllerBridge::
    GetOrCreateSessionHandleForQueryContextualizer() {
  return session_handle_.get();
}

void ComposeboxQueryControllerBridge::GetRelevantTabsForQuery(
    const std::string& query_text,
    const std::vector<GURL>& attached_context_urls,
    base::OnceCallback<void(
        std::vector<contextual_tasks::QueryContextualizer::TabId>)> callback) {
  std::move(callback).Run({});
}

void ComposeboxQueryControllerBridge::NotifySessionStarted(JNIEnv* env) {
  session_handle_->NotifySessionStarted();
}

void ComposeboxQueryControllerBridge::NotifySessionAbandoned(JNIEnv* env) {
  session_handle_->NotifySessionAbandoned();
}

base::android::ScopedJavaLocalRef<jobject>
ComposeboxQueryControllerBridge::AddFile(
    JNIEnv* env,
    const std::string& file_name,
    const std::string& file_type,
    const jni_zero::JavaRef<jobject>& file_data) {
  base::UnguessableToken file_token = session_handle_->CreateContextToken();

  std::optional<lens::ImageEncodingOptions> image_options = std::nullopt;
  if (file_type.find("image") != std::string::npos) {
    image_options = lens::ImageEncodingOptions{.enable_webp_encoding = false,
                                               .max_size = 1500000,
                                               .max_height = 1600,
                                               .max_width = 1600,
                                               .compression_quality = 40};
  } else if (file_type.find("pdf") != std::string::npos ||
             lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    AimEligibilityService* aim_service =
        AimEligibilityServiceFactory::GetForProfile(profile_);
    if (!aim_service->IsPdfUploadEligible()) {
      return {};
    }
  } else {
    // Unsupported mime type.
    return {};
  }

  base::span<const uint8_t> file_bytes_span =
      base::android::JavaByteBufferToSpan(env, file_data);
  session_handle_->StartFileContextUploadFlow(
      file_token, file_name, file_type, mojo_base::BigBuffer(file_bytes_span),
      std::move(image_options));

  return base::android::ConvertUTF8ToJavaString(env, file_token.ToString());
}

base::android::ScopedJavaLocalRef<jobject>
ComposeboxQueryControllerBridge::AddTabContext(
    JNIEnv* env,
    content::WebContents* web_contents,
    bool is_suggested_tab) {
  tabs::TabInterface* const tab =
      tabs::TabInterface::GetFromContents(web_contents);

  if (!tab) {
    return {};
  }

  lens::TabContextualizationController* tab_contextualization_controller =
      lens::TabContextualizationController::From(tab);
  if (!tab_contextualization_controller) {
    return {};
  }

  base::UnguessableToken file_token = session_handle_->CreateContextToken();
  // Leak this pointer it will delete itself when it's done.
  TabContextCaptureRequest* tab_context_capture = new TabContextCaptureRequest(
      tab_contextualization_controller, tab,
      base::BindOnce(
          &ComposeboxQueryControllerBridge::StartTabContextUploadFlow,
          weak_ptr_factory_.GetWeakPtr(), env, file_token,
          /*was_cached=*/false, base::TimeTicks::Now()));
  tab_context_capture->Start();

  return base::android::ConvertUTF8ToJavaString(env, file_token.ToString());
}

base::android::ScopedJavaLocalRef<jobject>
ComposeboxQueryControllerBridge::AddTabContextFromCache(JNIEnv* env,
                                                        long tab_id,
                                                        bool is_suggested_tab) {
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

  base::UnguessableToken file_token = session_handle_->CreateContextToken();

  cache->GetPageContentForTab(
      tab_id, base::BindOnce(
                  &ComposeboxQueryControllerBridge::OnGetPageContentFromCache,
                  weak_ptr_factory_.GetWeakPtr(), env, file_token,
                  base::TimeTicks::Now()));

  return base::android::ConvertUTF8ToJavaString(env, file_token.ToString());
}

std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
ComposeboxQueryControllerBridge::CreateSearchUrlRequestInfoFromUrl(
    GURL url,
    const std::string& query_text) {
  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
      search_url_request_info = std::make_unique<
          ComposeboxQueryController::CreateSearchUrlRequestInfo>();
  search_url_request_info->query_text = query_text;
  search_url_request_info->additional_params =
      lens::GetParametersMapWithoutQuery(url);
  search_url_request_info->query_start_time = base::Time::Now();
  search_url_request_info->aim_entry_point =
      omnibox::ANDROID_CHROME_FUSEBOX_ENTRY_POINT;
  return search_url_request_info;
}

void ComposeboxQueryControllerBridge::ContextualizeAndCreateSearchUrl(
    std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
        search_url_request_info,
    const base::android::JavaRef<jobject>& j_callback) {
  std::string query_text = search_url_request_info->query_text;

  auto callback = base::BindOnce(
      &contextual_search::ContextualSearchSessionHandle::CreateSearchUrl,
      session_handle_->AsWeakPtr(), std::move(search_url_request_info),
      base::BindOnce(&RunJavaCallback,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));

  query_contextualizer_->Contextualize(
      /*task_id=*/std::nullopt, query_text, /*tabs_to_recontextualize=*/{},
      /*tabs_to_force_contextualize=*/{},
      /*on_ineligible_callback=*/base::DoNothing(),
      /*on_processed_callback=*/base::DoNothing(),
      base::BindOnce(
          [](base::OnceClosure closure,
             base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
                 ignored_handle) { std::move(closure).Run(); },
          std::move(callback)),
      /*enable_smart_tab_selection=*/false);
}

void ComposeboxQueryControllerBridge::GetAimUrl(
    JNIEnv* env,
    GURL url,
    const std::string& query_text,
    const base::android::JavaRef<jobject>& j_callback) {
  ContextualizeAndCreateSearchUrl(
      CreateSearchUrlRequestInfoFromUrl(std::move(url), query_text),
      j_callback);
}

void ComposeboxQueryControllerBridge::GetImageGenerationUrl(
    JNIEnv* env,
    GURL url,
    const std::string& query_text,
    const base::android::JavaRef<jobject>& j_callback) {
  auto search_url_request_info =
      CreateSearchUrlRequestInfoFromUrl(std::move(url), query_text);
  search_url_request_info->additional_params["imgn"] = "1";
  ContextualizeAndCreateSearchUrl(std::move(search_url_request_info),
                                  j_callback);
}

void ComposeboxQueryControllerBridge::GetAimUrlFromInputState(
    JNIEnv* env,
    GURL url,
    const std::string& query_text,
    const base::android::JavaRef<jobject>& j_callback) {
  auto search_url_request_info =
      CreateSearchUrlRequestInfoFromUrl(std::move(url), query_text);

  if (input_state_model_) {
    for (const auto& [key, value] :
         input_state_model_->GetAdditionalQueryParams()) {
      DCHECK(!search_url_request_info->additional_params.contains(key));
      search_url_request_info->additional_params[key] = value;
    }
  }

  ContextualizeAndCreateSearchUrl(std::move(search_url_request_info),
                                  j_callback);
}

void ComposeboxQueryControllerBridge::RemoveAttachment(
    JNIEnv* env,
    const std::string& token) {
  std::optional<base::UnguessableToken> unguessable_token =
      base::UnguessableToken::DeserializeFromString(token);
  if (unguessable_token.has_value()) {
    session_handle_->DeleteFile(unguessable_token.value());
    if (input_state_model_) {
      input_state_model_->OnContextChanged();
    }
  }
}

bool ComposeboxQueryControllerBridge::IsFuseboxEligible(JNIEnv* env) {
  AimEligibilityService* aim_service =
      AimEligibilityServiceFactory::GetForProfile(profile_);
  return aim_service && aim_service->IsFuseboxEligible();
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

void ComposeboxQueryControllerBridge::SetActiveTool(
    JNIEnv* env,
    omnibox::ToolMode tool_mode) {
  if (input_state_model_) {
    input_state_model_->setActiveTool(tool_mode);
  }
}

void ComposeboxQueryControllerBridge::SetActiveModel(
    JNIEnv* env,
    omnibox::ModelMode model_mode) {
  if (input_state_model_) {
    input_state_model_->setActiveModel(model_mode);
  }
}

std::unique_ptr<lens::proto::LensOverlaySuggestInputs>
ComposeboxQueryControllerBridge::CreateLensOverlaySuggestInputs() const {
  std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
      session_handle_->GetSuggestInputs();
  if (!suggest_inputs.has_value()) {
    return std::make_unique<lens::proto::LensOverlaySuggestInputs>();
  }
  return std::make_unique<lens::proto::LensOverlaySuggestInputs>(
      *suggest_inputs);
}

void ComposeboxQueryControllerBridge::OnContextUploadStatusChanged(
    const base::UnguessableToken& context_token,
    lens::MimeType mime_type,
    contextual_search::ContextUploadStatus context_upload_status,
    const std::optional<contextual_search::ContextUploadErrorType>&
        error_type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ComposeboxQueryControllerBridge_onContextUploadStatusChanged(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env, context_token.ToString()),
      static_cast<int>(context_upload_status));

  if (input_state_model_) {
    input_state_model_->OnContextChanged();
  }
}

void ComposeboxQueryControllerBridge::OnGetPageContentFromCache(
    JNIEnv* env,
    const base::UnguessableToken& context_token,
    base::TimeTicks start_time,
    std::optional<optimization_guide::proto::PageContext> page_context) {
  // TODO(crbug.com/457869241): Merge this and the code in
  // TabContextualizationController.
  if (!page_context.has_value()) {
    OnContextUploadStatusChanged(
        context_token, lens::MimeType::kUnknown,
        contextual_search::ContextUploadStatus::kValidationFailed,
        contextual_search::ContextUploadErrorType::kBrowserProcessingError);
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

  StartTabContextUploadFlow(env, context_token, /*was_cached=*/true, start_time,
                            std::move(input_data));
}

void ComposeboxQueryControllerBridge::StartTabContextUploadFlow(
    JNIEnv* env,
    const base::UnguessableToken& context_token,
    bool was_cached,
    base::TimeTicks start_time,
    std::unique_ptr<lens::ContextualInputData> page_content_data) {
  if (!page_content_data || !page_content_data->context_input.has_value() ||
      page_content_data->context_input->size() <= 0) {
    OnContextUploadStatusChanged(
        context_token, lens::MimeType::kUnknown,
        contextual_search::ContextUploadStatus::kValidationFailed,
        contextual_search::ContextUploadErrorType::kBrowserProcessingError);
    return;
  }
  base::UmaHistogramMediumTimes(
      was_cached
          ? "Android.ComposeboxQueryController.AddTabContextCachedDuration"
          : "Android.ComposeboxQueryController.AddTabContextDuration",
      base::TimeTicks::Now() - start_time);

  std::optional<lens::ImageEncodingOptions> image_options =
      lens::ImageEncodingOptions{.enable_webp_encoding = false,
                                 .max_size = 1500000,
                                 .max_height = 1600,
                                 .max_width = 1600,
                                 .compression_quality = 40};

  session_handle_->StartTabContextUploadFlow(
      context_token, std::move(page_content_data), std::move(image_options));
}

void ComposeboxQueryControllerBridge::OnInputStateChanged(
    const contextual_search::InputState& state) {
  JNIEnv* env = base::android::AttachCurrentThread();

  std::vector<omnibox::InputType> max_inputs_by_types_keys;
  std::vector<int> max_inputs_by_types_values;
  for (const auto& [key, value] : state.max_inputs_by_type) {
    max_inputs_by_types_keys.emplace_back(key);
    max_inputs_by_types_values.emplace_back(value);
  }

  std::vector<std::vector<uint8_t>> tool_configs;
  for (const auto& config : state.tool_configs) {
    std::string serialized;
    config.SerializeToString(&serialized);
    tool_configs.emplace_back(
        std::vector<uint8_t>(serialized.begin(), serialized.end()));
  }

  std::vector<std::vector<uint8_t>> model_configs;
  for (const auto& config : state.model_configs) {
    std::string serialized;
    config.SerializeToString(&serialized);
    model_configs.emplace_back(
        std::vector<uint8_t>(serialized.begin(), serialized.end()));
  }

  std::vector<std::vector<uint8_t>> input_type_configs;
  for (const auto& config : state.input_type_configs) {
    std::string serialized;
    config.SerializeToString(&serialized);
    input_type_configs.emplace_back(
        std::vector<uint8_t>(serialized.begin(), serialized.end()));
  }

  std::vector<uint8_t> tools_section_config;
  if (state.tools_section_config.has_value()) {
    std::string serialized;
    state.tools_section_config->SerializeToString(&serialized);
    tools_section_config.assign(serialized.begin(), serialized.end());
  }

  std::vector<uint8_t> model_section_config;
  if (state.model_section_config.has_value()) {
    std::string serialized;
    state.model_section_config->SerializeToString(&serialized);
    model_section_config.assign(serialized.begin(), serialized.end());
  }

  base::android::ScopedJavaLocalRef<jobject> j_input_state =
      contextual_search::Java_InputState_Constructor(
          env, state.hint_text, state.allowed_input_types,
          state.disabled_input_types, state.max_total_inputs,
          max_inputs_by_types_keys, max_inputs_by_types_values,
          base::android::ToJavaArrayOfByteArray(env, input_type_configs),
          state.active_tool, state.allowed_tools, state.disabled_tools,
          state.image_gen_upload_active,
          base::android::ToJavaArrayOfByteArray(env, tool_configs),
          tools_section_config, state.active_model, state.GetDefaultModel(),
          state.allowed_models, state.disabled_models,
          base::android::ToJavaArrayOfByteArray(env, model_configs),
          model_section_config);

  Java_ComposeboxQueryControllerBridge_onInputStateChanged(env, java_obj_,
                                                           j_input_state);
}

void ComposeboxQueryControllerBridge::ResetInputStateModel() {
  input_state_model_.reset();
}

void ComposeboxQueryControllerBridge::UpdateSuggestedTabContext(
    const contextual_tasks::SuggestedTabInfo* suggested_tab) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<base::android::ScopedJavaLocalRef<jobject>> j_suggested_tabs;
  if (suggested_tab) {
    j_suggested_tabs.push_back(
        contextual_tasks::Java_SuggestedTabInfo_Constructor(
            env, suggested_tab->tab_id,
            base::android::ConvertUTF16ToJavaString(env, suggested_tab->title),
            url::GURLAndroid::FromNativeGURL(env, suggested_tab->url),
            suggested_tab->last_active.since_origin().InMilliseconds()));
  }

  Java_ComposeboxQueryControllerBridge_onSuggestedTabsUpdated(
      env, java_obj_,
      base::android::ToJavaArrayOfObjects(
          env,
          base::android::GetClass(
              env,
              "org/chromium/chrome/browser/omnibox/fusebox/SuggestedTabInfo")
              .obj(),
          j_suggested_tabs));
}

void ComposeboxQueryControllerBridge::OnTaskChanged() {
  // TODO(crbug.com/493281303): Implement this.
}

void ComposeboxQueryControllerBridge::InitializeInputStateModel() {
  if (OmniboxFieldTrial::kOmniboxShowModelPicker.Get()) {
    AimEligibilityService* aim_service =
        AimEligibilityServiceFactory::GetForProfile(profile_);
    auto* ui_service = profile_
                           ? contextual_tasks::ContextualTasksUiServiceFactory::
                                 GetForBrowserContext(profile_)
                           : nullptr;
    bool browser_identity_matches_aim_identity =
        ui_service && ui_service->IsSignedInToBrowserWithValidCredentials() &&
        ui_service->IsUrlForPrimaryAccount(GURL());
    const omnibox::SearchboxConfig* config_ptr =
        aim_service->GetSearchboxConfig();
    input_state_model_ = std::make_unique<contextual_search::InputStateModel>(
        *session_handle_, config_ptr ? *config_ptr : omnibox::SearchboxConfig(),
        GURL(), profile_ ? profile_->IsOffTheRecord() : false,
        browser_identity_matches_aim_identity);
    input_state_subscription_ =
        input_state_model_->subscribe(base::BindRepeating(
            &ComposeboxQueryControllerBridge::OnInputStateChanged,
            weak_ptr_factory_.GetWeakPtr()));
    input_state_model_->Initialize();
  }
}

void ComposeboxQueryControllerBridge::UpdateModelFromUrl(const GURL& url) {
  if (input_state_model_) {
    input_state_model_->UpdateModelFromUrl(url);
  }
}

void ComposeboxQueryControllerBridge::SubmitQueryToAimPage(
    JNIEnv* env,
    const std::string& query) {
  if (!contextual_tasks_web_ui_interface_) {
    return;
  }

  omnibox::ToolMode active_tool = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
  omnibox::ModelMode active_model = omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
  if (input_state_model_) {
    contextual_search::InputState input_state =
        input_state_model_->GetInputState();
    active_tool = input_state.active_tool;
    active_model = input_state.active_model;
  }

  auto callback = base::BindOnce(
      [](base::WeakPtr<ComposeboxQueryControllerBridge> self,
         const std::string& query, omnibox::ToolMode active_tool,
         omnibox::ModelMode active_model,
         base::WeakPtr<contextual_search::ContextualSearchSessionHandle>
             session_handle) {
        if (!self || !self->contextual_tasks_web_ui_interface_) {
          return;
        }
        auto request_info = contextual_tasks::PrepareClientToAimRequestInfo(
            query, self->session_handle_.get(),
            self->contextual_tasks_web_ui_interface_, active_tool, active_model,
            /*active_tab_context_id=*/std::nullopt,
            /*overlay_token=*/std::nullopt);

        contextual_tasks::FinalizeAndSendAimQuery(
            std::move(request_info), self->session_handle_.get(),
            self->contextual_tasks_web_ui_interface_);
      },
      weak_ptr_factory_.GetWeakPtr(), query, active_tool, active_model);

  query_contextualizer_->Contextualize(
      /*task_id=*/std::nullopt, query, /*tabs_to_recontextualize=*/{},
      /*tabs_to_force_contextualize=*/{},
      /*on_ineligible_callback=*/base::DoNothing(),
      /*on_processed_callback=*/base::DoNothing(), std::move(callback),
      /*enable_smart_tab_selection=*/false);
}

DEFINE_JNI(ComposeboxQueryControllerBridge)
