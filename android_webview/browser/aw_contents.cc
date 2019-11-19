// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents.h"

#include <limits>
#include <memory>
#include <utility>

#include "android_webview/browser/aw_autofill_client.h"
#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_contents_lifecycle_notifier.h"
#include "android_webview/browser/aw_pdf_exporter.h"
#include "android_webview/browser/aw_render_process.h"
#include "android_webview/browser/aw_renderer_priority.h"
#include "android_webview/browser/aw_resource_context.h"
#include "android_webview/browser/aw_settings.h"
#include "android_webview/browser/aw_web_contents_delegate.h"
#include "android_webview/browser/gfx/aw_gl_functor.h"
#include "android_webview/browser/gfx/aw_picture.h"
#include "android_webview/browser/gfx/browser_view_renderer.h"
#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/gpu_service_web_view.h"
#include "android_webview/browser/gfx/java_browser_view_renderer_helper.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "android_webview/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "android_webview/browser/permission/aw_permission_request.h"
#include "android_webview/browser/permission/permission_request_handler.h"
#include "android_webview/browser/permission/simple_permission_request.h"
#include "android_webview/browser/state_serializer.h"
#include "android_webview/browser_jni_headers/AwContents_jni.h"
#include "android_webview/common/aw_hit_test_data.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/devtools_instrumentation.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/android/autofill_provider_android.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "net/base/auth.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
struct AwDrawSWFunctionTable;

using autofill::AutofillManager;
using autofill::ContentAutofillDriverFactory;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::HasException;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;
using navigation_interception::InterceptNavigationDelegate;

namespace android_webview {

class CompositorFrameConsumer;

namespace {

bool g_should_download_favicons = false;

std::string* g_locale() {
  static base::NoDestructor<std::string> locale;
  return locale.get();
}

std::string* g_locale_list() {
  static base::NoDestructor<std::string> locale_list;
  return locale_list.get();
}

const void* const kAwContentsUserDataKey = &kAwContentsUserDataKey;
const void* const kComputedRendererPriorityUserDataKey =
    &kComputedRendererPriorityUserDataKey;

class AwContentsUserData : public base::SupportsUserData::Data {
 public:
  explicit AwContentsUserData(AwContents* ptr) : contents_(ptr) {}

  static AwContents* GetContents(WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    AwContentsUserData* data = static_cast<AwContentsUserData*>(
        web_contents->GetUserData(kAwContentsUserDataKey));
    return data ? data->contents_ : NULL;
  }

 private:
  AwContents* contents_;
};

base::subtle::Atomic32 g_instance_count = 0;

void JavaScriptResultCallbackForTesting(
    const ScopedJavaGlobalRef<jobject>& callback,
    base::Value result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  Java_AwContents_onEvaluateJavaScriptResultForTesting(env, j_json, callback);
}

}  // namespace

class ScopedAllowInitGLBindings {
 public:
  ScopedAllowInitGLBindings() {}

  ~ScopedAllowInitGLBindings() {}

 private:
  base::ScopedAllowBlocking allow_blocking_;
};

// static
AwContents* AwContents::FromWebContents(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return AwContentsUserData::GetContents(web_contents);
}

// static
void JNI_AwContents_UpdateDefaultLocale(
    JNIEnv* env,
    const JavaParamRef<jstring>& locale,
    const JavaParamRef<jstring>& locale_list) {
  *g_locale() = ConvertJavaStringToUTF8(env, locale);
  *g_locale_list() = ConvertJavaStringToUTF8(env, locale_list);
}

// static
std::string AwContents::GetLocale() {
  return *g_locale();
}

// static
std::string AwContents::GetLocaleList() {
  return *g_locale_list();
}

// static
AwBrowserPermissionRequestDelegate* AwBrowserPermissionRequestDelegate::FromID(
    int render_process_id,
    int render_frame_id) {
  AwContents* aw_contents =
      AwContents::FromWebContents(content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id,
                                           render_frame_id)));
  return aw_contents;
}

// static
AwSafeBrowsingUIManager::UIManagerClient*
AwSafeBrowsingUIManager::UIManagerClient::FromWebContents(
    WebContents* web_contents) {
  return AwContents::FromWebContents(web_contents);
}

// static
AwRenderProcessGoneDelegate* AwRenderProcessGoneDelegate::FromWebContents(
    content::WebContents* web_contents) {
  return AwContents::FromWebContents(web_contents);
}

AwContents::AwContents(std::unique_ptr<WebContents> web_contents)
    : content::WebContentsObserver(web_contents.get()),
      browser_view_renderer_(
          this,
          base::CreateSingleThreadTaskRunner({BrowserThread::UI})),
      web_contents_(std::move(web_contents)) {
  base::subtle::NoBarrier_AtomicIncrement(&g_instance_count, 1);
  icon_helper_.reset(new IconHelper(web_contents_.get()));
  icon_helper_->SetListener(this);
  web_contents_->SetUserData(android_webview::kAwContentsUserDataKey,
                             std::make_unique<AwContentsUserData>(this));
  browser_view_renderer_.RegisterWithWebContents(web_contents_.get());

  viz::FrameSinkId frame_sink_id;
  if (web_contents_->GetRenderViewHost()) {
    frame_sink_id =
        web_contents_->GetRenderViewHost()->GetWidget()->GetFrameSinkId();
  }

  browser_view_renderer_.SetActiveFrameSinkId(frame_sink_id);
  render_view_host_ext_.reset(
      new AwRenderViewHostExt(this, web_contents_.get()));

  InitializePageLoadMetricsForWebContents(web_contents_.get());

  permission_request_handler_.reset(
      new PermissionRequestHandler(this, web_contents_.get()));

  AwAutofillClient* autofill_manager_delegate =
      AwAutofillClient::FromWebContents(web_contents_.get());
  if (autofill_manager_delegate)
    InitAutofillIfNecessary(autofill_manager_delegate->GetSaveFormData());
  content::SynchronousCompositor::SetClientForWebContents(
      web_contents_.get(), &browser_view_renderer_);
  AwContentsLifecycleNotifier::OnWebViewCreated();
}

void AwContents::SetJavaPeers(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& aw_contents,
    const JavaParamRef<jobject>& web_contents_delegate,
    const JavaParamRef<jobject>& contents_client_bridge,
    const JavaParamRef<jobject>& io_thread_client,
    const JavaParamRef<jobject>& intercept_navigation_delegate,
    const JavaParamRef<jobject>& autofill_provider) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // The |aw_content| param is technically spurious as it duplicates |obj| but
  // is passed over anyway to make the binding more explicit.
  java_ref_ = JavaObjectWeakGlobalRef(env, aw_contents);

  web_contents_delegate_.reset(
      new AwWebContentsDelegate(env, web_contents_delegate));
  web_contents_->SetDelegate(web_contents_delegate_.get());

  contents_client_bridge_.reset(
      new AwContentsClientBridge(env, contents_client_bridge));
  AwContentsClientBridge::Associate(web_contents_.get(),
                                    contents_client_bridge_.get());

  AwContentsIoThreadClient::Associate(web_contents_.get(), io_thread_client);

  InterceptNavigationDelegate::Associate(
      web_contents_.get(), std::make_unique<InterceptNavigationDelegate>(
                               env, intercept_navigation_delegate));

  if (!autofill_provider.is_null()) {
    autofill_provider_ = std::make_unique<autofill::AutofillProviderAndroid>(
        autofill_provider, web_contents_.get());
  }
}

void AwContents::SetSaveFormData(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  InitAutofillIfNecessary(enabled);
  // We need to check for the existence, since autofill_manager_delegate
  // may not be created when the setting is false.
  if (AwAutofillClient::FromWebContents(web_contents_.get())) {
    AwAutofillClient::FromWebContents(web_contents_.get())
        ->SetSaveFormData(enabled);
  }
}

void AwContents::InitAutofillIfNecessary(bool autocomplete_enabled) {
  // Check if the autofill driver factory already exists.
  content::WebContents* web_contents = web_contents_.get();
  if (ContentAutofillDriverFactory::FromWebContents(web_contents))
    return;

  // Check if AutofillProvider is available.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // Just return, if the app neither runs on O sdk nor enables autocomplete.
  if (!autofill_provider_ && !autocomplete_enabled)
    return;

  AwAutofillClient::CreateForWebContents(web_contents);
  ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
      web_contents, AwAutofillClient::FromWebContents(web_contents),
      base::android::GetDefaultLocaleString(),
      AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
      autofill_provider_.get());
}

void AwContents::SetAwAutofillClient(const JavaRef<jobject>& client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_setAwAutofillClient(env, obj, client);
}

AwContents::~AwContents() {
  DCHECK_EQ(this, AwContents::FromWebContents(web_contents_.get()));
  web_contents_->RemoveUserData(kAwContentsUserDataKey);
  if (find_helper_.get())
    find_helper_->SetListener(NULL);
  if (icon_helper_.get())
    icon_helper_->SetListener(NULL);
  base::subtle::Atomic32 instance_count =
      base::subtle::NoBarrier_AtomicIncrement(&g_instance_count, -1);
  // When the last WebView is destroyed free all discardable memory allocated by
  // Chromium, because the app process may continue to run for a long time
  // without ever using another WebView.
  if (instance_count == 0) {
    // TODO(timvolodine): consider moving NotifyMemoryPressure to
    // AwContentsLifecycleNotifier (crbug.com/522988).
    base::MemoryPressureListener::NotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  }
  browser_view_renderer_.SetCurrentCompositorFrameConsumer(nullptr);
  AwContentsLifecycleNotifier::OnWebViewDestroyed();
}

base::android::ScopedJavaLocalRef<jobject> AwContents::GetWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents_);
  if (!web_contents_)
    return base::android::ScopedJavaLocalRef<jobject>();

  return web_contents_->GetJavaWebContents();
}

base::android::ScopedJavaLocalRef<jobject> AwContents::GetBrowserContext(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents_)
    return base::android::ScopedJavaLocalRef<jobject>();
  return AwBrowserContext::FromWebContents(web_contents_.get())
      ->GetJavaBrowserContext();
}

void AwContents::SetCompositorFrameConsumer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong compositor_frame_consumer) {
  browser_view_renderer_.SetCurrentCompositorFrameConsumer(
      reinterpret_cast<CompositorFrameConsumer*>(compositor_frame_consumer));
}

ScopedJavaLocalRef<jobject> AwContents::GetRenderProcess(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderProcessHost* host =
      web_contents_->GetMainFrame()->GetProcess();
  if (host->run_renderer_in_process()) {
    return ScopedJavaLocalRef<jobject>();
  }
  AwRenderProcess* render_process =
      AwRenderProcess::GetInstanceForRenderProcessHost(host);
  return render_process->GetJavaObject();
}

void AwContents::Destroy(JNIEnv* env) {
  java_ref_.reset();
  delete this;
}

static jlong JNI_AwContents_Init(JNIEnv* env, jlong browser_context_pointer) {
  AwBrowserContext* browser_context =
      reinterpret_cast<AwBrowserContext*>(browser_context_pointer);
  std::unique_ptr<WebContents> web_contents(content::WebContents::Create(
      content::WebContents::CreateParams(browser_context)));
  // Return an 'uninitialized' instance; most work is deferred until the
  // subsequent SetJavaPeers() call.
  return reinterpret_cast<intptr_t>(new AwContents(std::move(web_contents)));
}

static jboolean JNI_AwContents_HasRequiredHardwareExtensions(JNIEnv* env) {
  ScopedAllowInitGLBindings scoped_allow_init_gl_bindings;
  // Make sure GPUInfo is collected. This will initialize GL bindings,
  // collect GPUInfo, and compute GpuFeatureInfo if they have not been
  // already done.
  return GpuServiceWebView::GetInstance()
      ->gpu_info()
      .can_support_threaded_texture_mailbox;
}

static void JNI_AwContents_SetAwDrawSWFunctionTable(JNIEnv* env,
                                                    jlong function_table) {
  RasterHelperSetAwDrawSWFunctionTable(
      reinterpret_cast<AwDrawSWFunctionTable*>(function_table));
}

static void JNI_AwContents_SetAwDrawGLFunctionTable(JNIEnv* env,
                                                    jlong function_table) {}

// static
jint JNI_AwContents_GetNativeInstanceCount(JNIEnv* env) {
  return base::subtle::NoBarrier_Load(&g_instance_count);
}

// static
ScopedJavaLocalRef<jstring> JNI_AwContents_GetSafeBrowsingLocaleForTesting(
    JNIEnv* env) {
  ScopedJavaLocalRef<jstring> locale =
      ConvertUTF8ToJavaString(env, base::i18n::GetConfiguredLocale());
  return locale;
}

namespace {
void DocumentHasImagesCallback(const ScopedJavaGlobalRef<jobject>& message,
                               bool has_images) {
  Java_AwContents_onDocumentHasImagesResponse(AttachCurrentThread(), has_images,
                                              message);
}
}  // namespace

void AwContents::DocumentHasImages(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   const JavaParamRef<jobject>& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ScopedJavaGlobalRef<jobject> j_message;
  j_message.Reset(env, message);
  render_view_host_ext_->DocumentHasImages(
      base::BindOnce(&DocumentHasImagesCallback, j_message));
}

namespace {
void GenerateMHTMLCallback(const JavaRef<jobject>& callback,
                           const base::FilePath& path,
                           int64_t size) {
  JNIEnv* env = AttachCurrentThread();
  // Android files are UTF8, so the path conversion below is safe.
  Java_AwContents_generateMHTMLCallback(
      env, ConvertUTF8ToJavaString(env, path.AsUTF8Unsafe()), size, callback);
}
}  // namespace

void AwContents::GenerateMHTML(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               const JavaParamRef<jstring>& jpath,
                               const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::FilePath target_path(ConvertJavaStringToUTF8(env, jpath));
  web_contents_->GenerateMHTML(
      content::MHTMLGenerationParams(target_path),
      base::BindOnce(&GenerateMHTMLCallback,
                     ScopedJavaGlobalRef<jobject>(env, callback), target_path));
}

void AwContents::CreatePdfExporter(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   const JavaParamRef<jobject>& pdfExporter) {
  pdf_exporter_.reset(new AwPdfExporter(env, pdfExporter, web_contents_.get()));
}

bool AwContents::OnReceivedHttpAuthRequest(const JavaRef<jobject>& handler,
                                           const std::string& host,
                                           const std::string& realm) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;

  ScopedJavaLocalRef<jstring> jhost = ConvertUTF8ToJavaString(env, host);
  ScopedJavaLocalRef<jstring> jrealm = ConvertUTF8ToJavaString(env, realm);
  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "onReceivedHttpAuthRequest");
  Java_AwContents_onReceivedHttpAuthRequest(env, obj, handler, jhost, jrealm);
  return true;
}

void AwContents::SetOffscreenPreRaster(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.SetOffscreenPreRaster(enabled);
}

void AwContents::AddVisitedLinks(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobjectArray>& jvisited_links) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<base::string16> visited_link_strings;
  base::android::AppendJavaStringArrayToStringVector(env, jvisited_links,
                                                     &visited_link_strings);

  std::vector<GURL> visited_link_gurls;
  std::vector<base::string16>::const_iterator itr;
  for (itr = visited_link_strings.begin(); itr != visited_link_strings.end();
       ++itr) {
    visited_link_gurls.push_back(GURL(*itr));
  }

  AwBrowserContext::FromWebContents(web_contents_.get())
      ->AddVisitedURLs(visited_link_gurls);
}

namespace {

void ShowGeolocationPromptHelperTask(const JavaObjectWeakGlobalRef& java_ref,
                                     const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_ref = java_ref.get(env);
  if (j_ref.obj()) {
    ScopedJavaLocalRef<jstring> j_origin(
        ConvertUTF8ToJavaString(env, origin.spec()));
    devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
        "onGeolocationPermissionsShowPrompt");
    Java_AwContents_onGeolocationPermissionsShowPrompt(env, j_ref, j_origin);
  }
}

void ShowGeolocationPromptHelper(const JavaObjectWeakGlobalRef& java_ref,
                                 const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  if (java_ref.get(env).obj()) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ShowGeolocationPromptHelperTask, java_ref, origin));
  }
}

}  // anonymous namespace

void AwContents::ShowGeolocationPrompt(
    const GURL& requesting_frame,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL origin = requesting_frame.GetOrigin();
  bool show_prompt = pending_geolocation_prompts_.empty();
  pending_geolocation_prompts_.emplace_back(origin, std::move(callback));
  if (show_prompt) {
    ShowGeolocationPromptHelper(java_ref_, origin);
  }
}

// Invoked from Java
void AwContents::InvokeGeolocationCallback(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean value,
    const JavaParamRef<jstring>& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (pending_geolocation_prompts_.empty())
    return;

  GURL callback_origin(base::android::ConvertJavaStringToUTF16(env, origin));
  if (callback_origin.GetOrigin() ==
      pending_geolocation_prompts_.front().first) {
    std::move(pending_geolocation_prompts_.front().second).Run(value);
    pending_geolocation_prompts_.pop_front();
    if (!pending_geolocation_prompts_.empty()) {
      ShowGeolocationPromptHelper(java_ref_,
                                  pending_geolocation_prompts_.front().first);
    }
  }
}

void AwContents::HideGeolocationPrompt(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool removed_current_outstanding_callback = false;
  std::list<OriginCallback>::iterator it = pending_geolocation_prompts_.begin();
  while (it != pending_geolocation_prompts_.end()) {
    if ((*it).first == origin.GetOrigin()) {
      if (it == pending_geolocation_prompts_.begin()) {
        removed_current_outstanding_callback = true;
      }
      it = pending_geolocation_prompts_.erase(it);
    } else {
      ++it;
    }
  }

  if (removed_current_outstanding_callback) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> j_ref = java_ref_.get(env);
    if (j_ref.obj()) {
      devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
          "onGeolocationPermissionsHidePrompt");
      Java_AwContents_onGeolocationPermissionsHidePrompt(env, j_ref);
    }
    if (!pending_geolocation_prompts_.empty()) {
      ShowGeolocationPromptHelper(java_ref_,
                                  pending_geolocation_prompts_.front().first);
    }
  }
}

void AwContents::OnPermissionRequest(
    base::android::ScopedJavaLocalRef<jobject> j_request,
    AwPermissionRequest* request) {
  DCHECK(!j_request.is_null());
  DCHECK(request);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_ref = java_ref_.get(env);
  if (j_ref.is_null()) {
    permission_request_handler_->CancelRequest(request->GetOrigin(),
                                               request->GetResources());
    return;
  }

  Java_AwContents_onPermissionRequest(env, j_ref, j_request);
}

void AwContents::OnPermissionRequestCanceled(AwPermissionRequest* request) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_request = request->GetJavaObject();
  ScopedJavaLocalRef<jobject> j_ref = java_ref_.get(env);
  if (j_request.is_null() || j_ref.is_null())
    return;

  Java_AwContents_onPermissionRequestCanceled(env, j_ref, j_request);
}

void AwContents::PreauthorizePermission(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        const JavaParamRef<jstring>& origin,
                                        jlong resources) {
  permission_request_handler_->PreauthorizePermission(
      GURL(base::android::ConvertJavaStringToUTF8(env, origin)), resources);
}

void AwContents::RequestProtectedMediaIdentifierPermission(
    const GURL& origin,
    base::OnceCallback<void(bool)> callback) {
  permission_request_handler_->SendRequest(
      std::make_unique<SimplePermissionRequest>(
          origin, AwPermissionRequest::ProtectedMediaId, std::move(callback)));
}

void AwContents::CancelProtectedMediaIdentifierPermissionRequests(
    const GURL& origin) {
  permission_request_handler_->CancelRequest(
      origin, AwPermissionRequest::ProtectedMediaId);
}

void AwContents::RequestGeolocationPermission(
    const GURL& origin,
    base::OnceCallback<void(bool)> callback) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  if (Java_AwContents_useLegacyGeolocationPermissionAPI(env, obj)) {
    ShowGeolocationPrompt(origin, std::move(callback));
    return;
  }
  permission_request_handler_->SendRequest(
      std::make_unique<SimplePermissionRequest>(
          origin, AwPermissionRequest::Geolocation, std::move(callback)));
}

void AwContents::CancelGeolocationPermissionRequests(const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  if (Java_AwContents_useLegacyGeolocationPermissionAPI(env, obj)) {
    HideGeolocationPrompt(origin);
    return;
  }
  permission_request_handler_->CancelRequest(origin,
                                             AwPermissionRequest::Geolocation);
}

void AwContents::RequestMIDISysexPermission(
    const GURL& origin,
    base::OnceCallback<void(bool)> callback) {
  permission_request_handler_->SendRequest(
      std::make_unique<SimplePermissionRequest>(
          origin, AwPermissionRequest::MIDISysex, std::move(callback)));
}

void AwContents::CancelMIDISysexPermissionRequests(const GURL& origin) {
  permission_request_handler_->CancelRequest(
      origin, AwPermissionRequest::AwPermissionRequest::MIDISysex);
}

void AwContents::FindAllAsync(JNIEnv* env,
                              const JavaParamRef<jobject>& obj,
                              const JavaParamRef<jstring>& search_string) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->FindAllAsync(ConvertJavaStringToUTF16(env, search_string));
}

void AwContents::FindNext(JNIEnv* env,
                          const JavaParamRef<jobject>& obj,
                          jboolean forward) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->FindNext(forward);
}

void AwContents::ClearMatches(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->ClearMatches();
}

void AwContents::ClearCache(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            jboolean include_disk_files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  render_view_host_ext_->ClearCache();

  if (include_disk_files) {
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(
            web_contents_->GetBrowserContext());
    remover->Remove(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_CACHE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);
  }
}

void AwContents::KillRenderProcess(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  render_view_host_ext_->KillRenderProcess();
}

FindHelper* AwContents::GetFindHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!find_helper_.get()) {
    find_helper_.reset(new FindHelper(web_contents_.get()));
    find_helper_->SetListener(this);
  }
  return find_helper_.get();
}

bool AwContents::AllowThirdPartyCookies() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents_.get());
  return aw_settings->GetAllowThirdPartyCookies();
}

void AwContents::OnFindResultReceived(int active_ordinal,
                                      int match_count,
                                      bool finished) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_onFindResultReceived(env, obj, active_ordinal, match_count,
                                       finished);
}

bool AwContents::ShouldDownloadFavicon(const GURL& icon_url) {
  return g_should_download_favicons;
}

void AwContents::OnReceivedIcon(const GURL& icon_url, const SkBitmap& bitmap) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  content::NavigationEntry* entry =
      web_contents_->GetController().GetLastCommittedEntry();

  if (entry) {
    entry->GetFavicon().valid = true;
    entry->GetFavicon().url = icon_url;
    entry->GetFavicon().image = gfx::Image::CreateFrom1xBitmap(bitmap);
  }

  Java_AwContents_onReceivedIcon(env, obj, gfx::ConvertToJavaBitmap(&bitmap));
}

void AwContents::OnReceivedTouchIconUrl(const std::string& url,
                                        bool precomposed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_AwContents_onReceivedTouchIconUrl(
      env, obj, ConvertUTF8ToJavaString(env, url), precomposed);
}

void AwContents::PostInvalidate() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_AwContents_postInvalidateOnAnimation(env, obj);
}

void AwContents::OnNewPicture() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
        "onNewPicture");
    Java_AwContents_onNewPicture(env, obj);
  }
}

void AwContents::OnViewTreeForceDarkStateChanged(
    bool view_tree_force_dark_state) {
  view_tree_force_dark_state_ = view_tree_force_dark_state;
  web_contents_->NotifyPreferencesChanged();
}

base::android::ScopedJavaLocalRef<jbyteArray> AwContents::GetCertificate(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NavigationEntry* entry =
      web_contents_->GetController().GetLastCommittedEntry();
  if (!entry || !entry->GetSSL().certificate)
    return ScopedJavaLocalRef<jbyteArray>();

  // Convert the certificate and return it
  base::StringPiece der_string = net::x509_util::CryptoBufferAsStringPiece(
      entry->GetSSL().certificate->cert_buffer());
  return base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(der_string.data()),
      der_string.length());
}

void AwContents::RequestNewHitTestDataAt(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jfloat x,
                                         jfloat y,
                                         jfloat touch_major) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gfx::PointF touch_center(x, y);
  gfx::SizeF touch_area(touch_major, touch_major);
  render_view_host_ext_->RequestNewHitTestDataAt(touch_center, touch_area);
}

void AwContents::UpdateLastHitTestData(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!render_view_host_ext_->HasNewHitTestData())
    return;

  const AwHitTestData& data = render_view_host_ext_->GetLastHitTestData();
  render_view_host_ext_->MarkHitTestDataRead();

  // Make sure to null the Java object if data is empty/invalid.
  ScopedJavaLocalRef<jstring> extra_data_for_type;
  if (data.extra_data_for_type.length())
    extra_data_for_type =
        ConvertUTF8ToJavaString(env, data.extra_data_for_type);

  ScopedJavaLocalRef<jstring> href;
  if (data.href.length())
    href = ConvertUTF16ToJavaString(env, data.href);

  ScopedJavaLocalRef<jstring> anchor_text;
  if (data.anchor_text.length())
    anchor_text = ConvertUTF16ToJavaString(env, data.anchor_text);

  ScopedJavaLocalRef<jstring> img_src;
  if (data.img_src.is_valid())
    img_src = ConvertUTF8ToJavaString(env, data.img_src.spec());

  Java_AwContents_updateHitTestData(env, obj, data.type, extra_data_for_type,
                                    href, anchor_text, img_src);
}

void AwContents::OnSizeChanged(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               int w,
                               int h,
                               int ow,
                               int oh) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gfx::Size size(w, h);
  web_contents_->GetNativeView()->OnPhysicalBackingSizeChanged(size);
  web_contents_->GetNativeView()->OnSizeChanged(w, h);
  browser_view_renderer_.OnSizeChanged(w, h);
}

void AwContents::SetViewVisibility(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   bool visible) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.SetViewVisibility(visible);
}

void AwContents::SetWindowVisibility(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     bool visible) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.SetWindowVisibility(visible);
}

void AwContents::SetIsPaused(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             bool paused) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.SetIsPaused(paused);
}

void AwContents::OnAttachedToWindow(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    int w,
                                    int h) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.OnAttachedToWindow(w, h);
}

void AwContents::OnDetachedFromWindow(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.OnDetachedFromWindow();
}

bool AwContents::IsVisible(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return browser_view_renderer_.IsClientVisible();
}

base::android::ScopedJavaLocalRef<jbyteArray> AwContents::GetOpaqueState(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Required optimization in WebViewClassic to not save any state if
  // there has been no navigations.
  if (!web_contents_->GetController().GetEntryCount())
    return ScopedJavaLocalRef<jbyteArray>();

  base::Pickle pickle;
  WriteToPickle(*web_contents_, &pickle);
  return base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(pickle.data()), pickle.size());
}

jboolean AwContents::RestoreFromOpaqueState(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jbyteArray>& state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(boliu): This copy can be optimized out if this is a performance
  // problem.
  std::vector<uint8_t> state_vector;
  base::android::JavaByteArrayToByteVector(env, state, &state_vector);

  base::Pickle pickle(reinterpret_cast<const char*>(state_vector.data()),
                      state_vector.size());
  base::PickleIterator iterator(pickle);

  return RestoreFromPickle(&iterator, web_contents_.get());
}

bool AwContents::OnDraw(JNIEnv* env,
                        const JavaParamRef<jobject>& obj,
                        const JavaParamRef<jobject>& canvas,
                        jboolean is_hardware_accelerated,
                        jint scroll_x,
                        jint scroll_y,
                        jint visible_left,
                        jint visible_top,
                        jint visible_right,
                        jint visible_bottom,
                        jboolean force_auxiliary_bitmap_rendering) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gfx::Vector2d scroll(scroll_x, scroll_y);
  browser_view_renderer_.PrepareToDraw(
      scroll, gfx::Rect(visible_left, visible_top, visible_right - visible_left,
                        visible_bottom - visible_top));
  if (is_hardware_accelerated && browser_view_renderer_.attached_to_window() &&
      !force_auxiliary_bitmap_rendering) {
    return browser_view_renderer_.OnDrawHardware();
  }

  gfx::Size view_size = browser_view_renderer_.size();
  if (view_size.IsEmpty()) {
    TRACE_EVENT_INSTANT0("android_webview", "EarlyOut_EmptySize",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // TODO(hush): Right now webview size is passed in as the auxiliary bitmap
  // size, which might hurt performance (only for software draws with auxiliary
  // bitmap). For better performance, get global visible rect, transform it
  // from screen space to view space, then intersect with the webview in
  // viewspace.  Use the resulting rect as the auxiliary bitmap.
  std::unique_ptr<SoftwareCanvasHolder> canvas_holder =
      SoftwareCanvasHolder::Create(canvas, scroll, view_size,
                                   force_auxiliary_bitmap_rendering);
  if (!canvas_holder || !canvas_holder->GetCanvas()) {
    TRACE_EVENT_INSTANT0("android_webview", "EarlyOut_NoSoftwareCanvas",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  return browser_view_renderer_.OnDrawSoftware(canvas_holder->GetCanvas());
}

bool AwContents::NeedToDrawBackgroundColor(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return browser_view_renderer_.NeedToDrawBackgroundColor();
}

void AwContents::SetPendingWebContentsForPopup(
    std::unique_ptr<content::WebContents> pending) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (pending_contents_.get()) {
    // TODO(benm): Support holding multiple pop up window requests.
    LOG(WARNING) << "Blocking popup window creation as an outstanding "
                 << "popup window is still pending.";
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    pending.release());
    return;
  }
  pending_contents_.reset(new AwContents(std::move(pending)));
  // Set dip_scale for pending contents, which is necessary for the later
  // SynchronousCompositor and InputHandler setup.
  pending_contents_->SetDipScaleInternal(browser_view_renderer_.dip_scale());
}

void AwContents::FocusFirstNode(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents_->FocusThroughTabTraversal(false);
}

void AwContents::SetBackgroundColor(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jint color) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  render_view_host_ext_->SetBackgroundColor(color);
}

void AwContents::ZoomBy(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jfloat delta) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.ZoomBy(delta);
}

void AwContents::OnComputeScroll(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 jlong animation_time_millis) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.OnComputeScroll(
      base::TimeTicks() +
      base::TimeDelta::FromMilliseconds(animation_time_millis));
}

jlong AwContents::ReleasePopupAwContents(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(pending_contents_.release());
}

gfx::Point AwContents::GetLocationOnScreen() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return gfx::Point();
  std::vector<int> location;
  base::android::JavaIntArrayToIntVector(
      env, Java_AwContents_getLocationOnScreen(env, obj), &location);
  return gfx::Point(location[0], location[1]);
}

void AwContents::ScrollContainerViewTo(const gfx::Vector2d& new_value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_scrollContainerViewTo(env, obj, new_value.x(), new_value.y());
}

void AwContents::UpdateScrollState(const gfx::Vector2d& max_scroll_offset,
                                   const gfx::SizeF& contents_size_dip,
                                   float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_updateScrollState(
      env, obj, max_scroll_offset.x(), max_scroll_offset.y(),
      contents_size_dip.width(), contents_size_dip.height(), page_scale_factor,
      min_page_scale_factor, max_page_scale_factor);
}

void AwContents::DidOverscroll(const gfx::Vector2d& overscroll_delta,
                               const gfx::Vector2dF& overscroll_velocity) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_didOverscroll(env, obj, overscroll_delta.x(),
                                overscroll_delta.y(), overscroll_velocity.x(),
                                overscroll_velocity.y());
}

ui::TouchHandleDrawable* AwContents::CreateDrawable() {
  JNIEnv* env = AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return nullptr;
  return reinterpret_cast<ui::TouchHandleDrawable*>(
      Java_AwContents_onCreateTouchHandle(env, obj));
}

void AwContents::SetDipScale(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             jfloat dip_scale) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SetDipScaleInternal(dip_scale);
}

void AwContents::SetDipScaleInternal(float dip_scale) {
  browser_view_renderer_.SetDipScale(dip_scale);
}

void AwContents::ScrollTo(JNIEnv* env,
                          const JavaParamRef<jobject>& obj,
                          jint x,
                          jint y) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.ScrollTo(gfx::Vector2d(x, y));
}

void AwContents::RestoreScrollAfterTransition(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jint x,
                                              jint y) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.RestoreScrollAfterTransition(gfx::Vector2d(x, y));
}

void AwContents::SmoothScroll(JNIEnv* env,
                              const JavaParamRef<jobject>& obj,
                              jint target_x,
                              jint target_y,
                              jlong duration_ms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  float scale = browser_view_renderer_.page_scale_factor();
  if (!content::IsUseZoomForDSFEnabled())
    scale *= browser_view_renderer_.dip_scale();

  DCHECK_GE(duration_ms, 0);
  render_view_host_ext_->SmoothScroll(
      target_x / scale, target_y / scale,
      base::TimeDelta::FromMilliseconds(duration_ms));
}

void AwContents::OnWebLayoutPageScaleFactorChanged(float page_scale_factor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_onWebLayoutPageScaleFactorChanged(env, obj,
                                                    page_scale_factor);
}

void AwContents::OnWebLayoutContentsSizeChanged(
    const gfx::Size& contents_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  gfx::Size contents_size_css =
      content::IsUseZoomForDSFEnabled()
          ? ScaleToRoundedSize(contents_size,
                               1 / browser_view_renderer_.dip_scale())
          : contents_size;
  Java_AwContents_onWebLayoutContentsSizeChanged(
      env, obj, contents_size_css.width(), contents_size_css.height());
}

jlong AwContents::CapturePicture(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 int width,
                                 int height) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(
      new AwPicture(browser_view_renderer_.CapturePicture(width, height)));
}

void AwContents::EnableOnNewPicture(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jboolean enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.EnableOnNewPicture(enabled);
}

namespace {
void InvokeVisualStateCallback(const JavaObjectWeakGlobalRef& java_ref,
                               jlong request_id,
                               const JavaRef<jobject>& callback,
                               bool result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref.get(env);
  if (obj.is_null())
    return;
  Java_AwContents_invokeVisualStateCallback(env, obj, callback, request_id);
}
}  // namespace

void AwContents::InsertVisualStateCallback(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong request_id,
    const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents_->GetMainFrame()->InsertVisualStateCallback(
      base::Bind(&InvokeVisualStateCallback, java_ref_, request_id,
                 ScopedJavaGlobalRef<jobject>(env, callback)));
}

jint AwContents::GetEffectivePriority(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  switch (
      web_contents_->GetMainFrame()->GetProcess()->GetEffectiveImportance()) {
    case content::ChildProcessImportance::NORMAL:
      return static_cast<jint>(RendererPriority::WAIVED);
    case content::ChildProcessImportance::MODERATE:
      return static_cast<jint>(RendererPriority::LOW);
    case content::ChildProcessImportance::IMPORTANT:
      return static_cast<jint>(RendererPriority::HIGH);
  }
  NOTREACHED();
  return 0;
}

JsJavaConfiguratorHost* AwContents::GetJsJavaConfiguratorHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!js_java_configurator_host_.get()) {
    js_java_configurator_host_ =
        std::make_unique<JsJavaConfiguratorHost>(web_contents_.get());
  }
  return js_java_configurator_host_.get();
}

base::android::ScopedJavaLocalRef<jstring> AwContents::AddWebMessageListener(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& listener,
    const base::android::JavaParamRef<jstring>& js_object_name,
    const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules) {
  return GetJsJavaConfiguratorHost()->AddWebMessageListener(
      env, listener, js_object_name, allowed_origin_rules);
}

void AwContents::RemoveWebMessageListener(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& js_object_name) {
  GetJsJavaConfiguratorHost()->RemoveWebMessageListener(env, js_object_name);
}

void AwContents::ClearView(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.ClearView();
}

void AwContents::SetExtraHeadersForUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& jextra_headers) {
  std::string extra_headers;
  if (jextra_headers)
    extra_headers = ConvertJavaStringToUTF8(env, jextra_headers);
  AwResourceContext* resource_context = static_cast<AwResourceContext*>(
      AwBrowserContext::FromWebContents(web_contents_.get())
          ->GetResourceContext());
  resource_context->SetExtraHeaders(GURL(ConvertJavaStringToUTF8(env, url)),
                                    extra_headers);
}

void AwContents::SetJsOnlineProperty(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     jboolean network_up) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  render_view_host_ext_->SetJsOnlineProperty(network_up);
}

void AwContents::TrimMemory(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            jint level,
                            jboolean visible) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Constants from Android ComponentCallbacks2.
  enum {
    TRIM_MEMORY_RUNNING_LOW = 10,
    TRIM_MEMORY_UI_HIDDEN = 20,
    TRIM_MEMORY_BACKGROUND = 40,
    TRIM_MEMORY_MODERATE = 60,
  };

  // Not urgent enough. TRIM_MEMORY_UI_HIDDEN is treated specially because
  // it does not indicate memory pressure, but merely that the app is
  // backgrounded.
  if (level < TRIM_MEMORY_RUNNING_LOW || level == TRIM_MEMORY_UI_HIDDEN)
    return;

  // Do not release resources on view we expect to get DrawGL soon.
  if (level < TRIM_MEMORY_BACKGROUND && visible)
    return;

  browser_view_renderer_.TrimMemory();
}

void AwContents::GrantFileSchemeAccesstoChildProcess(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
      web_contents_->GetMainFrame()->GetProcess()->GetID(), url::kFileScheme);
}

void AwContents::ResumeLoadingCreatedPopupWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  web_contents_->ResumeLoadingCreatedWebContents();
}

jlong AwContents::GetAutofillProvider(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return reinterpret_cast<jlong>(autofill_provider_.get());
}

void JNI_AwContents_SetShouldDownloadFavicons(JNIEnv* env) {
  g_should_download_favicons = true;
}

void AwContents::RenderViewHostChanged(content::RenderViewHost* old_host,
                                       content::RenderViewHost* new_host) {
  DCHECK(new_host);

  // At this point, the current RVH may or may not contain a compositor. So
  // compositor_ may be nullptr, in which case
  // BrowserViewRenderer::DidInitializeCompositor() callback is time when the
  // new compositor is constructed.
  browser_view_renderer_.SetActiveFrameSinkId(
      new_host->GetWidget()->GetFrameSinkId());
}

void AwContents::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If this request was blocked in any way, broadcast an error.
  net::Error error_code = navigation_handle->GetNetErrorCode();
  if (error_code != net::ERR_BLOCKED_BY_CLIENT &&
      error_code != net::ERR_BLOCKED_BY_ADMINISTRATOR &&
      error_code != net::ERR_ABORTED) {
    return;
  }
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents_.get());
  if (!client)
    return;

  AwWebResourceRequest request(navigation_handle->GetURL().spec(),
                               navigation_handle->IsPost() ? "POST" : "GET",
                               navigation_handle->IsInMainFrame(),
                               navigation_handle->HasUserGesture(),
                               net::HttpRequestHeaders());
  request.is_renderer_initiated = navigation_handle->IsRendererInitiated();

  client->OnReceivedError(request, error_code, false);
}

void AwContents::DidAttachInterstitialPage() {
  RenderFrameHost* rfh = web_contents_->GetInterstitialPage()->GetMainFrame();
  browser_view_renderer_.SetActiveFrameSinkId(
      rfh->GetRenderViewHost()->GetWidget()->GetFrameSinkId());
}

void AwContents::DidDetachInterstitialPage() {
  if (!web_contents_)
    return;
  browser_view_renderer_.SetActiveFrameSinkId(
      web_contents_->GetRenderViewHost()->GetWidget()->GetFrameSinkId());
}

bool AwContents::CanShowInterstitial() {
  JNIEnv* env = AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_AwContents_canShowInterstitial(env, obj);
}

int AwContents::GetErrorUiType() {
  JNIEnv* env = AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  return Java_AwContents_getErrorUiType(env, obj);
}

void AwContents::EvaluateJavaScriptOnInterstitialForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& script,
    const base::android::JavaParamRef<jobject>& callback) {
  content::InterstitialPage* interstitial =
      web_contents_->GetInterstitialPage();
  DCHECK(interstitial);

  if (!callback) {
    // No callback requested.
    interstitial->GetMainFrame()->ExecuteJavaScriptForTests(
        ConvertJavaStringToUTF16(env, script), base::NullCallback());
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  RenderFrameHost::JavaScriptResultCallback js_callback =
      base::BindOnce(&JavaScriptResultCallbackForTesting, j_callback);

  interstitial->GetMainFrame()->ExecuteJavaScriptForTests(
      ConvertJavaStringToUTF16(env, script), std::move(js_callback));
}

void AwContents::RendererUnresponsive(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  AwRenderProcess* aw_render_process =
      AwRenderProcess::GetInstanceForRenderProcessHost(render_process_host);
  Java_AwContents_onRendererUnresponsive(env, obj,
                                         aw_render_process->GetJavaObject());
}

void AwContents::RendererResponsive(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  AwRenderProcess* aw_render_process =
      AwRenderProcess::GetInstanceForRenderProcessHost(render_process_host);
  Java_AwContents_onRendererResponsive(env, obj,
                                       aw_render_process->GetJavaObject());
}

AwContents::RenderProcessGoneResult AwContents::OnRenderProcessGone(
    int child_process_id,
    bool crashed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return RenderProcessGoneResult::kHandled;

  bool result =
      Java_AwContents_onRenderProcessGone(env, obj, child_process_id, crashed);

  if (HasException(env))
    return RenderProcessGoneResult::kException;

  return result ? RenderProcessGoneResult::kHandled
                : RenderProcessGoneResult::kUnhandled;
}

}  // namespace android_webview
