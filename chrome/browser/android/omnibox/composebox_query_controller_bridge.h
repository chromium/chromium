// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_string.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_search/internal/composebox_query_controller.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {
class WebContents;
}  //  namespace content

namespace contextual_tasks {
class ContextualTasksUIInterface;
}  // namespace contextual_tasks

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

class Profile;
class GURL;

class ComposeboxQueryControllerBridge
    : public ComposeboxQueryController::ContextUploadStatusObserver,
      public contextual_tasks::ContextualTasksComposeboxHandlerInterface,
      public contextual_tasks::QueryContextualizer::Delegate {
 public:
  explicit ComposeboxQueryControllerBridge(
      const base::android::JavaRef<jobject>& java_obj,
      Profile* profile,
      content::WebContents* web_contents,
      bool is_task_scoped);
  ~ComposeboxQueryControllerBridge() override;
  void Destroy(JNIEnv* env);
  void OnWebUIDestroyed(JNIEnv* env);
  void NotifySessionStarted(JNIEnv* env);
  void NotifySessionAbandoned(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> AddFile(
      JNIEnv* env,
      const std::string& file_name,
      const std::string& file_type,
      const jni_zero::JavaRef<jobject>& file_data);
  base::android::ScopedJavaLocalRef<jobject> AddTabContext(
      JNIEnv* env,
      content::WebContents* web_contents,
      bool is_suggested_tab);
  base::android::ScopedJavaLocalRef<jobject>
  AddTabContextFromCache(JNIEnv* env, long tab_id, bool is_suggested_tab);
  void GetAimUrl(JNIEnv* env,
                 GURL url,
                 const std::string& query_text,
                 const base::android::JavaRef<jobject>& j_callback);
  void GetImageGenerationUrl(JNIEnv* env,
                             GURL url,
                             const std::string& query_text,
                             const base::android::JavaRef<jobject>& j_callback);

  // Builds the URL to use for a navigation, supplementing the passed in URL
  // with additional parameters. This will do things such as include the current
  // tool (image gen, deep search, etc). The input state will be polled for this
  // information. This reduces client side knowledge of how to use tools (and
  // soon models), as the information of what parameters to add is coming from
  // the server.
  void GetAimUrlFromInputState(
      JNIEnv* env,
      GURL url,
      const std::string& query_text,
      const base::android::JavaRef<jobject>& j_callback);
  void RemoveAttachment(JNIEnv* env, const std::string& token);
  bool IsFuseboxEligible(JNIEnv* env);
  bool IsPdfUploadEligible(JNIEnv* env);
  bool IsCreateImagesEligible(JNIEnv* env);
  void SetActiveTool(JNIEnv* env, omnibox::ToolMode tool_mode);
  void SetActiveModel(JNIEnv* env, omnibox::ModelMode model_mode);
  void SubmitQueryToAimPage(JNIEnv* env, const std::string& query);

  std::unique_ptr<lens::proto::LensOverlaySuggestInputs>
  CreateLensOverlaySuggestInputs() const;

  // ComposeboxQueryController::ContextUploadStatusObserver:
  void OnContextUploadStatusChanged(
      const base::UnguessableToken& context_token,
      lens::MimeType mime_type,
      contextual_search::ContextUploadStatus context_upload_status,
      const std::optional<contextual_search::ContextUploadErrorType>&
          error_type) override;

  size_t GetAttachmentCount() const;

  base::WeakPtr<ComposeboxQueryControllerBridge> AsWeakPtr();

  // contextual_tasks::ContextualTasksComposeboxHandlerInterface:
  void ResetInputStateModel() override;
  void UpdateSuggestedTabContext(
      const contextual_tasks::SuggestedTabInfo* suggested_tab) override;
  void OnTaskChanged() override;
  void InitializeInputStateModel() override;
  void UpdateModelFromUrl(const GURL& url) override;

  // contextual_tasks::QueryContextualizer::Delegate:
  GURL GetTabUrl(contextual_tasks::QueryContextualizer::TabId id) override;
  SessionID GetTabSessionId(
      contextual_tasks::QueryContextualizer::TabId id) override;
  void GetPageContext(
      contextual_tasks::QueryContextualizer::TabId id,
      base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
          callback) override;
  bool IsTabValid(contextual_tasks::QueryContextualizer::TabId id) override;
  std::optional<lens::ImageEncodingOptions>
  GetTabViewportEncodingOptionsForQueryContextualizer() override;
  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateSessionHandleForQueryContextualizer() override;
  void GetRelevantTabsForQuery(
      const std::string& query_text,
      const std::vector<GURL>& attached_context_urls,
      base::OnceCallback<void(
          std::vector<contextual_tasks::QueryContextualizer::TabId>)> callback)
      override;

 private:
  void OnGetPageContentFromCache(
      JNIEnv* env,
      const base::UnguessableToken& context_token,
      base::TimeTicks start_time,
      std::optional<optimization_guide::proto::PageContext> page_context);
  void StartTabContextUploadFlow(
      JNIEnv* env,
      const base::UnguessableToken& context_token,
      bool was_cached,
      base::TimeTicks start_time,
      std::unique_ptr<lens::ContextualInputData> page_content_data);
  void OnInputStateChanged(const contextual_search::InputState& state);

  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
  CreateSearchUrlRequestInfoFromUrl(GURL url, const std::string& query_text);
  void ContextualizeAndCreateSearchUrl(
      std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
          search_url_request_info,
      const base::android::JavaRef<jobject>& j_callback);
  contextual_search::ContextualSearchContextController* query_controller()
      const {
    return session_handle_->GetController();
  }

  raw_ptr<Profile> profile_;
  raw_ptr<contextual_tasks::ContextualTasksUIInterface>
      contextual_tasks_web_ui_interface_ = nullptr;
  bool is_task_scoped_ = false;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  std::unique_ptr<contextual_search::InputStateModel> input_state_model_;
  base::CallbackListSubscription input_state_subscription_;
  std::unique_ptr<contextual_tasks::QueryContextualizer> query_contextualizer_;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  base::WeakPtrFactory<ComposeboxQueryControllerBridge> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_
