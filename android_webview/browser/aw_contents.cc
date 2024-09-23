// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents.h"

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "android_webview/browser/aw_app_defined_websites.h"
#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_pdf_exporter.h"
#include "android_webview/browser/aw_render_process.h"
#include "android_webview/browser/aw_renderer_priority.h"
#include "android_webview/browser/aw_settings.h"
#include "android_webview/browser/aw_web_contents_delegate.h"
#include "android_webview/browser/gfx/aw_gl_functor.h"
#include "android_webview/browser/gfx/aw_picture.h"
#include "android_webview/browser/gfx/browser_view_renderer.h"
#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/java_browser_view_renderer_helper.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "android_webview/browser/js_java_interaction/aw_web_message_host_factory.h"
#include "android_webview/browser/lifecycle/aw_contents_lifecycle_notifier.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "android_webview/browser/permission/aw_permission_request.h"
#include "android_webview/browser/permission/permission_callback.h"
#include "android_webview/browser/permission/permission_request_handler.h"
#include "android_webview/browser/permission/simple_permission_request.h"
#include "android_webview/browser/state_serializer.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/devtools_instrumentation.h"
#include "android_webview/common/mojom/frame.mojom.h"
#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "components/android_autofill/browser/android_autofill_client.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/android_autofill_provider.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/js_injection/browser/js_communication_host.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/sensitive_content/android/android_sensitive_content_client.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/mhtml_generation_params.h"
#include "net/base/auth.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"
#include "url/url_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwContents_jni.h"
#include "android_webview/browser_jni_headers/AwSiteVisitLogger_jni.h"
#include "android_webview/browser_jni_headers/StartupJavascriptInfo_jni.h"

struct AwDrawSWFunctionTable;

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::HasException;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;
using js_injection::JsCommunicationHost;
using navigation_interception::InterceptNavigationDelegate;
using NotRestoredReason = content::BackForwardCache::NotRestoredReason;

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused
enum class StorageAccessAppDefinedType {
  kAppDefined = 0,
  kExternal = 1,
  kMaxValue = kExternal,
};

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
    return data ? data->contents_.get() : NULL;
  }

 private:
  raw_ptr<AwContents> contents_;
};

base::subtle::Atomic32 g_instance_count = 0;

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
      AwSafeBrowsingAllowlistSetObserver(
          AwBrowserProcess::GetInstance()->GetSafeBrowsingAllowlistManager()),
      browser_view_renderer_(this,
                             content::GetUIThreadTaskRunner({}),
                             content::GetIOThreadTaskRunner({})),
      web_contents_(std::move(web_contents)) {
  TRACE_EVENT_BEGIN("android_webview.timeline", "WebView Instance",
                    perfetto::Track::FromPointer(this));
  base::subtle::NoBarrier_AtomicIncrement(&g_instance_count, 1);
  icon_helper_ = std::make_unique<IconHelper>(web_contents_.get());
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
  render_view_host_ext_ =
      std::make_unique<AwRenderViewHostExt>(this, web_contents_.get());

  InitializePageLoadMetricsForWebContents(web_contents_.get());
  AwMetricsServiceClient::GetInstance()->OnWebContentsCreated(
      web_contents_.get());

  permission_request_handler_ =
      std::make_unique<PermissionRequestHandler>(this, web_contents_.get());

  auto* browser_context =
      AwBrowserContext::FromWebContents(web_contents_.get());

  // Using a separate URLLoaderFactory is preferable as this is an internal
  // request made by Android WebView that should not be subject to attribution
  // and interception logic common for navigation-related network activity.
  storage_access_url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
          browser_context->CreateURLLoaderFactory()));
  asset_link_handler_ = std::make_unique<
      content_relationship_verification::DigitalAssetLinksHandler>(
      storage_access_url_loader_factory_);

  content::SynchronousCompositor::SetClientForWebContents(
      web_contents_.get(), &browser_view_renderer_);
  AwContentsLifecycleNotifier::GetInstance().OnWebViewCreated(this);
  AwBrowserProcess::GetInstance()->visibility_metrics_logger()->AddClient(this);
}

void AwContents::SetJavaPeers(
    JNIEnv* env,
    const JavaParamRef<jobject>& aw_contents,
    const JavaParamRef<jobject>& web_contents_delegate,
    const JavaParamRef<jobject>& contents_client_bridge,
    const JavaParamRef<jobject>& io_thread_client,
    const JavaParamRef<jobject>& intercept_navigation_delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // The |aw_content| param is technically spurious as it duplicates |obj| but
  // is passed over anyway to make the binding more explicit.
  java_ref_ = JavaObjectWeakGlobalRef(env, aw_contents);

  web_contents_delegate_ =
      std::make_unique<AwWebContentsDelegate>(env, web_contents_delegate);
  web_contents_->SetDelegate(web_contents_delegate_.get());

  contents_client_bridge_ =
      std::make_unique<AwContentsClientBridge>(env, contents_client_bridge);
  AwContentsClientBridge::Associate(web_contents_.get(),
                                    contents_client_bridge_.get());

  AwContentsIoThreadClient::Associate(web_contents_.get(), io_thread_client);

  InterceptNavigationDelegate::Associate(
      web_contents_.get(), std::make_unique<InterceptNavigationDelegate>(
                               env, intercept_navigation_delegate));
}

void AwContents::InitializeAndroidAutofill(JNIEnv* env) {
  DCHECK(autofill::AutofillProvider::FromWebContents(web_contents_.get()));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (autofill::ContentAutofillClient::FromWebContents(web_contents_.get())) {
    return;
  }
  // The AutofillProvider object is already created by the AutofillProvider
  // Java object, except in tests.
  if (!autofill::AutofillProvider::FromWebContents(web_contents_.get())) {
    return;
  }
  android_autofill::AndroidAutofillClient::CreateForWebContents(
      web_contents_.get());

  // We need to initialize the keyboard suppressor before creating any
  // AutofillManagers and after the autofill client is available.
  autofill::AutofillProvider::FromWebContents(web_contents_.get())
      ->MaybeInitKeyboardSuppressor();
}

void AwContents::InitSensitiveContentClient(JNIEnv* env) {
  sensitive_content::AndroidSensitiveContentClient::CreateForWebContents(
      web_contents_.get(), "SensitiveContent.WebView.");
}

AwContents::~AwContents() {
  DCHECK_EQ(this, AwContents::FromWebContents(web_contents_.get()));
  web_contents_->RemoveUserData(kAwContentsUserDataKey);
  AwContentsClientBridge::Dissociate(web_contents_.get());
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
  AwContentsLifecycleNotifier::GetInstance().OnWebViewDestroyed(this);
  WebContentsObserver::Observe(nullptr);
  AwBrowserProcess::GetInstance()->visibility_metrics_logger()->RemoveClient(
      this);
  // Corresponds to "WebView Instance" in AwContents's constructor.
  TRACE_EVENT_END("android_webview.timeline",
                  perfetto::Track::FromPointer(this));
}

base::android::ScopedJavaLocalRef<jobject> AwContents::GetWebContents(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents_);
  if (!web_contents_)
    return base::android::ScopedJavaLocalRef<jobject>();

  return web_contents_->GetJavaWebContents();
}

base::android::ScopedJavaLocalRef<jobject> AwContents::GetBrowserContext(
    JNIEnv* env) {
  if (!web_contents_)
    return base::android::ScopedJavaLocalRef<jobject>();
  return AwBrowserContext::FromWebContents(web_contents_.get())
      ->GetJavaBrowserContext();
}

void AwContents::SetCompositorFrameConsumer(JNIEnv* env,
                                            jlong compositor_frame_consumer) {
  browser_view_renderer_.SetCurrentCompositorFrameConsumer(
      reinterpret_cast<CompositorFrameConsumer*>(compositor_frame_consumer));
}

ScopedJavaLocalRef<jobject> AwContents::GetRenderProcess(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderProcessHost* host =
      web_contents_->GetPrimaryMainFrame()->GetProcess();
  if (host->run_renderer_in_process()) {
    return ScopedJavaLocalRef<jobject>();
  }
  AwRenderProcess* render_process =
      AwRenderProcess::GetInstanceForRenderProcessHost(host);
  return render_process->GetJavaObject();
}

base::android::ScopedJavaLocalRef<jobject> AwContents::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return java_ref_.get(env);
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

static void JNI_AwContents_UpdateScreenCoverage(
    JNIEnv* env,
    jint global_percentage,
    const base::android::JavaParamRef<jobjectArray>& jschemes,
    const base::android::JavaParamRef<jintArray>& jscheme_percentages) {
  std::vector<std::string> schemes;
  AppendJavaStringArrayToStringVector(env, jschemes, &schemes);

  std::vector<int> scheme_percentages;
  JavaIntArrayToIntVector(env, jscheme_percentages, &scheme_percentages);

  DCHECK(schemes.size() == scheme_percentages.size());

  std::vector<VisibilityMetricsLogger::Scheme> scheme_enums(schemes.size());
  for (size_t i = 0; i < schemes.size(); i++) {
    scheme_enums[i] = VisibilityMetricsLogger::SchemeStringToEnum(schemes[i]);
  }

  AwBrowserProcess::GetInstance()
      ->visibility_metrics_logger()
      ->UpdateScreenCoverage(global_percentage, scheme_enums,
                             scheme_percentages);
}

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

static ScopedJavaLocalRef<jobject> JNI_AwContents_FromWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  base::android::ScopedJavaLocalRef<jobject> jaw_contents;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  AwContents* aw_contents =
      web_contents ? AwContents::FromWebContents(web_contents) : nullptr;
  if (aw_contents) {
    jaw_contents = aw_contents->GetJavaObject();
  }
  return jaw_contents;
}

namespace {
void DocumentHasImagesCallback(const ScopedJavaGlobalRef<jobject>& message,
                               bool has_images) {
  Java_AwContents_onDocumentHasImagesResponse(AttachCurrentThread(), has_images,
                                              message);
}
}  // namespace

void AwContents::DocumentHasImages(JNIEnv* env,
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
                                   const JavaParamRef<jobject>& pdfExporter) {
  pdf_exporter_ =
      std::make_unique<AwPdfExporter>(env, pdfExporter, web_contents_.get());
}

bool AwContents::OnReceivedHttpAuthRequest(const JavaRef<jobject>& handler,
                                           const std::string& host,
                                           const std::string& realm) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
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
    const JavaParamRef<jobjectArray>& jvisited_links) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<std::u16string> visited_link_strings;
  AppendJavaStringArrayToStringVector(env, jvisited_links,
                                      &visited_link_strings);

  std::vector<GURL> visited_link_gurls;
  std::vector<std::u16string>::const_iterator itr;
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
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ShowGeolocationPromptHelperTask, java_ref, origin));
  }
}

}  // anonymous namespace

void AwContents::ShowGeolocationPrompt(const GURL& requesting_frame,
                                       PermissionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL origin = requesting_frame.DeprecatedGetOriginAsURL();
  bool show_prompt = pending_geolocation_prompts_.empty();
  pending_geolocation_prompts_.emplace_back(origin, std::move(callback));
  if (show_prompt) {
    ShowGeolocationPromptHelper(java_ref_, origin);
  }
}

// Invoked from Java
void AwContents::InvokeGeolocationCallback(
    JNIEnv* env,
    jboolean value,
    const JavaParamRef<jstring>& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (pending_geolocation_prompts_.empty())
    return;

  GURL callback_origin(base::android::ConvertJavaStringToUTF16(env, origin));
  if (callback_origin.DeprecatedGetOriginAsURL() ==
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
    if ((*it).first == origin.DeprecatedGetOriginAsURL()) {
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
  DCHECK(j_request);
  DCHECK(request);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_ref = java_ref_.get(env);
  if (!j_ref) {
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
  if (!j_request || !j_ref)
    return;

  Java_AwContents_onPermissionRequestCanceled(env, j_ref, j_request);
}

void AwContents::PreauthorizePermission(JNIEnv* env,
                                        const JavaParamRef<jstring>& origin,
                                        jlong resources) {
  permission_request_handler_->PreauthorizePermission(
      GURL(base::android::ConvertJavaStringToUTF8(env, origin)), resources);
}

void AwContents::RequestProtectedMediaIdentifierPermission(
    const GURL& origin,
    PermissionCallback callback) {
  permission_request_handler_->SendRequest(
      std::make_unique<SimplePermissionRequest>(
          origin, AwPermissionRequest::ProtectedMediaId, std::move(callback)));
}

void AwContents::CancelProtectedMediaIdentifierPermissionRequests(
    const GURL& origin) {
  permission_request_handler_->CancelRequest(
      origin, AwPermissionRequest::ProtectedMediaId);
}

void AwContents::RequestGeolocationPermission(const GURL& origin,
                                              PermissionCallback callback) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  if (UseLegacyGeolocationPermissionAPI()) {
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
  if (!obj)
    return;

  if (UseLegacyGeolocationPermissionAPI()) {
    HideGeolocationPrompt(origin);
    return;
  }
  permission_request_handler_->CancelRequest(origin,
                                             AwPermissionRequest::Geolocation);
}

bool AwContents::UseLegacyGeolocationPermissionAPI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj) {
    return false;
  }

  return Java_AwContents_useLegacyGeolocationPermissionAPI(env, obj);
}

void AwContents::RequestMIDISysexPermission(const GURL& origin,
                                            PermissionCallback callback) {
  permission_request_handler_->SendRequest(
      std::make_unique<SimplePermissionRequest>(
          origin, AwPermissionRequest::MIDISysex, std::move(callback)));
}

void AwContents::CancelMIDISysexPermissionRequests(const GURL& origin) {
  permission_request_handler_->CancelRequest(
      origin, AwPermissionRequest::AwPermissionRequest::MIDISysex);
}


void AwContents::RequestStorageAccess(const url::Origin& top_level_origin,
                                      PermissionCallback callback) {
  base::TimeTicks time_requested = base::TimeTicks::Now();

  AppDefinedWebsites::GetInstance()->AppDeclaresDomainInAssetStatements(
      std::make_unique<AssetDomainListIncludeHandler>(
          storage_access_url_loader_factory_),
      top_level_origin,
      base::BindOnce(&AwContents::GrantRequestStorageAccessIfOriginIsAppDefined,
                     weak_ptr_factory_.GetWeakPtr(), top_level_origin,
                     time_requested, std::move(callback)));
}

void AwContents::GrantRequestStorageAccessIfOriginIsAppDefined(
    const url::Origin top_level_origin,
    base::TimeTicks time_requested,
    PermissionCallback callback,
    bool is_defined) {
  base::UmaHistogramEnumeration("Android.WebView.StorageAccessRelation2",
                                is_defined
                                    ? StorageAccessAppDefinedType::kAppDefined
                                    : StorageAccessAppDefinedType::kExternal);

  if (!base::FeatureList::IsEnabled(features::kWebViewAutoSAA)) {
    NOTIMPLEMENTED()
        << "RequestPermissions is not implemented for storage access";
    std::move(callback).Run(false);
    return;
  }

  if (!is_defined) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(crbug.com/355460995): We should investigate if we should have a
  // particular relation string from the android app side as well. For the
  // moment, we will just accept any string that the app declares, and then
  // verify the relation on the website's side.
  constexpr char kRelationship[] = "delegate_permission/common.handle_all_urls";
  asset_link_handler_->CheckDigitalAssetLinkRelationshipForAndroidApp(
      top_level_origin, kRelationship,
      std::vector<std::string>{
          base::android::BuildInfo::GetInstance()->host_signing_cert_sha256()},
      base::android::BuildInfo::GetInstance()->host_package_name(),
      base::BindOnce(
          [](base::TimeTicks time_requested, PermissionCallback callback,
             content_relationship_verification::RelationshipCheckResult
                 result) {
            const base::TimeTicks time_answered = base::TimeTicks::Now();
            base::UmaHistogramTimes(
                "Android.WebView.StorageAccessAutoGrantTime",
                time_answered - time_requested);
            std::move(callback).Run(result ==
                                    content_relationship_verification::
                                        RelationshipCheckResult::kSuccess);
          },
          time_requested, std::move(callback)));
}

void AwContents::FindAllAsync(JNIEnv* env,
                              const JavaParamRef<jstring>& search_string) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->FindAllAsync(ConvertJavaStringToUTF16(env, search_string));
}

void AwContents::FindNext(JNIEnv* env, jboolean forward) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->FindNext(forward);
}

void AwContents::ClearMatches(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->ClearMatches();
}

void AwContents::ClearCache(JNIEnv* env, jboolean include_disk_files) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AwRenderProcess* aw_render_process =
      AwRenderProcess::GetInstanceForRenderProcessHost(
          web_contents_->GetPrimaryMainFrame()->GetProcess());

  aw_render_process->ClearCache();

  if (include_disk_files) {
    content::BrowsingDataRemover* remover =
        web_contents_->GetBrowserContext()->GetBrowsingDataRemover();
    remover->Remove(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_CACHE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
            content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);
  }
}

FindHelper* AwContents::GetFindHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!find_helper_.get()) {
    find_helper_ = std::make_unique<FindHelper>(web_contents_.get());
    find_helper_->SetListener(this);
  }
  return find_helper_.get();
}

bool AwContents::IsJavaScriptAllowed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AwSettings* aw_settings = AwSettings::FromWebContents(web_contents_.get());
  return aw_settings->GetJavaScriptEnabled();
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
  if (!obj)
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
  if (!obj)
    return;

  content::NavigationEntry* entry =
      web_contents_->GetController().GetLastCommittedEntry();
  entry->GetFavicon().valid = true;
  entry->GetFavicon().url = icon_url;
  entry->GetFavicon().image = gfx::Image::CreateFrom1xBitmap(bitmap);

  ScopedJavaLocalRef<jobject> java_bitmap =
      gfx::ConvertToJavaBitmap(bitmap, gfx::OomBehavior::kReturnNullOnOom);
  if (!java_bitmap) {
    LOG(WARNING) << "Skipping onReceivedIcon; Not enough memory to convert "
                    "icon to Bitmap.";
    return;
  }
  Java_AwContents_onReceivedIcon(env, obj, java_bitmap);
}

void AwContents::OnReceivedTouchIconUrl(const std::string& url,
                                        bool precomposed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  Java_AwContents_onReceivedTouchIconUrl(
      env, obj, ConvertUTF8ToJavaString(env, url), precomposed);
}

void AwContents::PostInvalidate(bool inside_vsync) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj)
    Java_AwContents_postInvalidate(env, obj, inside_vsync);
}

void AwContents::OnNewPicture() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj) {
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

void AwContents::SetPreferredFrameInterval(
    base::TimeDelta preferred_frame_interval) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (preferred_frame_interval_ == preferred_frame_interval) {
    return;
  }
  preferred_frame_interval_ = preferred_frame_interval;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj) {
    Java_AwContents_onPreferredFrameIntervalChanged(
        env, obj, preferred_frame_interval.InNanoseconds());
  }
}

base::android::ScopedJavaLocalRef<jbyteArray> AwContents::GetCertificate(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NavigationEntry* entry =
      web_contents_->GetController().GetLastCommittedEntry();
  if (entry->IsInitialEntry() || !entry->GetSSL().certificate) {
    return ScopedJavaLocalRef<jbyteArray>();
  }

  // Convert the certificate and return it
  std::string_view der_string = net::x509_util::CryptoBufferAsStringPiece(
      entry->GetSSL().certificate->cert_buffer());
  return base::android::ToJavaByteArray(env, base::as_byte_span(der_string));
}

void AwContents::RequestNewHitTestDataAt(JNIEnv* env,
                                         jfloat x,
                                         jfloat y,
                                         jfloat touch_major) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gfx::PointF touch_center(x, y);
  gfx::SizeF touch_area(touch_major, touch_major);
  render_view_host_ext_->RequestNewHitTestDataAt(touch_center, touch_area);
}

void AwContents::UpdateLastHitTestData(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  android_webview::mojom::HitTestDataPtr data =
      render_view_host_ext_->TakeLastHitTestData();
  if (!data)
    return;

  // Make sure to null the Java object if data is empty/invalid.
  ScopedJavaLocalRef<jstring> extra_data_for_type;
  if (data->extra_data_for_type.length())
    extra_data_for_type =
        ConvertUTF8ToJavaString(env, data->extra_data_for_type);

  ScopedJavaLocalRef<jstring> href;
  if (data->href.length())
    href = ConvertUTF16ToJavaString(env, data->href);

  ScopedJavaLocalRef<jstring> anchor_text;
  if (data->anchor_text.length())
    anchor_text = ConvertUTF16ToJavaString(env, data->anchor_text);

  ScopedJavaLocalRef<jstring> img_src;
  if (data->img_src.is_valid())
    img_src = ConvertUTF8ToJavaString(env, data->img_src.spec());

  Java_AwContents_updateHitTestData(env, obj, static_cast<jint>(data->type),
                                    extra_data_for_type, href, anchor_text,
                                    img_src);
}

void AwContents::OnSizeChanged(JNIEnv* env, int w, int h, int ow, int oh) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gfx::Size size(w, h);
  web_contents_->GetNativeView()->OnPhysicalBackingSizeChanged(size);
  web_contents_->GetNativeView()->OnSizeChanged(w, h);
  browser_view_renderer_.OnSizeChanged(w, h);
  AwBrowserProcess::GetInstance()
      ->visibility_metrics_logger()
      ->ClientVisibilityChanged(this);
}

void AwContents::OnConfigurationChanged(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents()->OnWebPreferencesChanged();
}

void AwContents::SetViewVisibility(JNIEnv* env, bool visible) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.SetViewVisibility(visible);
  AwBrowserProcess::GetInstance()
      ->visibility_metrics_logger()
      ->ClientVisibilityChanged(this);
}

void AwContents::SetWindowVisibility(JNIEnv* env, bool visible) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.SetWindowVisibility(visible);
  if (visible)
    AwContentsLifecycleNotifier::GetInstance().OnWebViewWindowBeVisible(this);
  else
    AwContentsLifecycleNotifier::GetInstance().OnWebViewWindowBeInvisible(this);
  AwBrowserProcess::GetInstance()
      ->visibility_metrics_logger()
      ->ClientVisibilityChanged(this);
}

void AwContents::SetIsPaused(JNIEnv* env, bool paused) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.SetIsPaused(paused);
}

void AwContents::OnAttachedToWindow(JNIEnv* env, int w, int h) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.OnAttachedToWindow(w, h);
  AwContentsLifecycleNotifier::GetInstance().OnWebViewAttachedToWindow(this);
  AwBrowserProcess::GetInstance()
      ->visibility_metrics_logger()
      ->ClientVisibilityChanged(this);
}

void AwContents::OnDetachedFromWindow(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.OnDetachedFromWindow();
  AwContentsLifecycleNotifier::GetInstance().OnWebViewDetachedFromWindow(this);
  AwBrowserProcess::GetInstance()
      ->visibility_metrics_logger()
      ->ClientVisibilityChanged(this);
}

bool AwContents::IsVisible(JNIEnv* env) {
  return browser_view_renderer_.IsClientVisible();
}

bool AwContents::IsDisplayingInterstitialForTesting(JNIEnv* env) {
  security_interstitials::SecurityInterstitialTabHelper*
      security_interstitial_tab_helper = security_interstitials::
          SecurityInterstitialTabHelper::FromWebContents(web_contents_.get());
  return security_interstitial_tab_helper &&
         security_interstitial_tab_helper->IsDisplayingInterstitial();
}

base::android::ScopedJavaLocalRef<jbyteArray> AwContents::GetOpaqueState(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Required optimization in WebViewClassic to not save any state if
  // there has been no navigations.
  if (web_contents_->GetController()
          .GetLastCommittedEntry()
          ->IsInitialEntry()) {
    return ScopedJavaLocalRef<jbyteArray>();
  }

  base::Pickle pickle;
  WriteToPickle(*web_contents_, &pickle);
  return base::android::ToJavaByteArray(env, pickle);
}

jboolean AwContents::RestoreFromOpaqueState(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(boliu): This copy can be optimized out if this is a performance
  // problem.
  std::vector<uint8_t> state_vector;
  base::android::JavaByteArrayToByteVector(env, state, &state_vector);

  base::Pickle pickle = base::Pickle::WithUnownedBuffer(state_vector);
  base::PickleIterator iterator(pickle);

  return RestoreFromPickle(&iterator, web_contents_.get());
}

bool AwContents::OnDraw(JNIEnv* env,
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
  gfx::Point scroll(scroll_x, scroll_y);
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

jfloat AwContents::GetVelocityInPixelsPerSecond(JNIEnv* env) {
  return browser_view_renderer_.GetVelocityInPixelsPerSecond();
}

bool AwContents::NeedToDrawBackgroundColor(JNIEnv* env) {
  return browser_view_renderer_.NeedToDrawBackgroundColor();
}

void AwContents::SetPendingWebContentsForPopup(
    std::unique_ptr<content::WebContents> pending) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (pending_contents_.get()) {
    // TODO(benm): Support holding multiple pop up window requests.
    LOG(WARNING) << "Blocking popup window creation as an outstanding "
                 << "popup window is still pending.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, pending.release());
    return;
  }
  pending_contents_ = std::make_unique<AwContents>(std::move(pending));
  // Set dip_scale for pending contents, which is necessary for the later
  // SynchronousCompositor and InputHandler setup.
  pending_contents_->SetDipScaleInternal(browser_view_renderer_.dip_scale());
}

void AwContents::FocusFirstNode(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents_->FocusThroughTabTraversal(false);
}

void AwContents::SetBackgroundColor(JNIEnv* env, jint color) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents_->SetPageBaseBackgroundColor(color);
}

void AwContents::ZoomBy(JNIEnv* env, jfloat delta) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.ZoomBy(delta);
}

void AwContents::OnComputeScroll(JNIEnv* env, jlong animation_time_millis) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.OnComputeScroll(
      base::TimeTicks() + base::Milliseconds(animation_time_millis));
}

jlong AwContents::ReleasePopupAwContents(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(pending_contents_.release());
}

gfx::Point AwContents::GetLocationOnScreen() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return gfx::Point();
  std::vector<int> location;
  JavaIntArrayToIntVector(env, Java_AwContents_getLocationOnScreen(env, obj),
                          &location);
  return gfx::Point(location[0], location[1]);
}

void AwContents::ScrollContainerViewTo(const gfx::Point& new_value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;
  Java_AwContents_scrollContainerViewTo(env, obj, new_value.x(), new_value.y());
}

void AwContents::UpdateScrollState(const gfx::Point& max_scroll_offset,
                                   const gfx::SizeF& contents_size_dip,
                                   float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;
  Java_AwContents_updateScrollState(
      env, obj, max_scroll_offset.x(), max_scroll_offset.y(),
      contents_size_dip.width(), contents_size_dip.height(), page_scale_factor,
      min_page_scale_factor, max_page_scale_factor);
}

void AwContents::DidOverscroll(const gfx::Vector2d& overscroll_delta,
                               const gfx::Vector2dF& overscroll_velocity,
                               bool inside_vsync) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;
  Java_AwContents_didOverscroll(env, obj, overscroll_delta.x(),
                                overscroll_delta.y(), overscroll_velocity.x(),
                                overscroll_velocity.y(), inside_vsync);
}

ui::TouchHandleDrawable* AwContents::CreateDrawable() {
  JNIEnv* env = AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return nullptr;
  return reinterpret_cast<ui::TouchHandleDrawable*>(
      Java_AwContents_onCreateTouchHandle(env, obj));
}

void AwContents::SetDipScale(JNIEnv* env, jfloat dip_scale) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SetDipScaleInternal(dip_scale);
}

base::android::ScopedJavaLocalRef<jstring> AwContents::GetScheme(JNIEnv* env) {
  return ConvertUTF8ToJavaString(env, scheme_);
}

void AwContents::OnInputEvent(JNIEnv* env) {
  browser_view_renderer_.OnInputEvent();
}

void AwContents::SetDipScaleInternal(float dip_scale) {
  browser_view_renderer_.SetDipScale(dip_scale);
}

void AwContents::ScrollTo(JNIEnv* env, jint x, jint y) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.ScrollTo(gfx::Point(x, y));
}

void AwContents::RestoreScrollAfterTransition(JNIEnv* env, jint x, jint y) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.RestoreScrollAfterTransition(gfx::Point(x, y));
}

void AwContents::SmoothScroll(JNIEnv* env,
                              jint target_x,
                              jint target_y,
                              jlong duration_ms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  float scale = browser_view_renderer_.page_scale_factor();

  DCHECK_GE(duration_ms, 0);
  render_view_host_ext_->SmoothScroll(target_x / scale, target_y / scale,
                                      base::Milliseconds(duration_ms));
}

void AwContents::OnWebLayoutPageScaleFactorChanged(float page_scale_factor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;
  Java_AwContents_onWebLayoutPageScaleFactorChanged(env, obj,
                                                    page_scale_factor);
}

void AwContents::OnWebLayoutContentsSizeChanged(
    const gfx::Size& contents_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;
  gfx::Size contents_size_css =
      ScaleToRoundedSize(contents_size, 1 / browser_view_renderer_.dip_scale());
  Java_AwContents_onWebLayoutContentsSizeChanged(
      env, obj, contents_size_css.width(), contents_size_css.height());
}

jlong AwContents::CapturePicture(JNIEnv* env, int width, int height) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(
      new AwPicture(browser_view_renderer_.CapturePicture(width, height)));
}

void AwContents::EnableOnNewPicture(JNIEnv* env, jboolean enabled) {
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
  if (!obj)
    return;
  Java_AwContents_invokeVisualStateCallback(env, obj, callback, request_id);
}
}  // namespace

void AwContents::InsertVisualStateCallback(
    JNIEnv* env,
    jlong request_id,
    const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents_->GetPrimaryMainFrame()->InsertVisualStateCallback(
      base::BindOnce(&InvokeVisualStateCallback, java_ref_, request_id,
                     ScopedJavaGlobalRef<jobject>(env, callback)));
}

jint AwContents::GetEffectivePriority(JNIEnv* env) {
  switch (web_contents_->GetPrimaryMainFrame()
              ->GetProcess()
              ->GetEffectiveImportance()) {
    case content::ChildProcessImportance::NORMAL:
      return static_cast<jint>(RendererPriority::WAIVED);
    case content::ChildProcessImportance::MODERATE:
      return static_cast<jint>(RendererPriority::LOW);
    case content::ChildProcessImportance::IMPORTANT:
      return static_cast<jint>(RendererPriority::HIGH);
  }
  NOTREACHED();
}

JsCommunicationHost* AwContents::GetJsCommunicationHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!js_communication_host_.get()) {
    js_communication_host_ =
        std::make_unique<JsCommunicationHost>(web_contents_.get());
  }
  return js_communication_host_.get();
}

jint AwContents::AddDocumentStartJavaScript(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& script,
    const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules) {
  std::vector<std::string> native_allowed_origin_rule_strings;
  AppendJavaStringArrayToStringVector(env, allowed_origin_rules,
                                      &native_allowed_origin_rule_strings);
  web_contents()->GetController().GetBackForwardCache().Flush(
      NotRestoredReason::kWebViewDocumentStartJavascriptChanged);
  web_contents()->CancelAllPrerendering();
  auto result = GetJsCommunicationHost()->AddDocumentStartJavaScript(
      base::android::ConvertJavaStringToUTF16(env, script),
      native_allowed_origin_rule_strings);
  if (result.error_message) {
    env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                  result.error_message->data());
    return -1;
  }
  DCHECK(result.script_id);
  return result.script_id.value();
}

void AwContents::RemoveDocumentStartJavaScript(JNIEnv* env, jint script_id) {
  web_contents()->CancelAllPrerendering();
  GetJsCommunicationHost()->RemoveDocumentStartJavaScript(script_id);
}

base::android::ScopedJavaLocalRef<jstring> AwContents::AddWebMessageListener(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& listener,
    const base::android::JavaParamRef<jstring>& js_object_name,
    const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules) {
  std::u16string native_js_object_name =
      base::android::ConvertJavaStringToUTF16(env, js_object_name);
  std::vector<std::string> native_allowed_origin_rule_strings;
  AppendJavaStringArrayToStringVector(env, allowed_origin_rules,
                                      &native_allowed_origin_rule_strings);
  const std::u16string error_message =
      GetJsCommunicationHost()->AddWebMessageHostFactory(
          std::make_unique<AwWebMessageHostFactory>(listener),
          native_js_object_name, native_allowed_origin_rule_strings);
  if (error_message.empty())
    return nullptr;
  return base::android::ConvertUTF16ToJavaString(env, error_message);
}

void AwContents::RemoveWebMessageListener(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& js_object_name) {
  GetJsCommunicationHost()->RemoveWebMessageHostFactory(
      ConvertJavaStringToUTF16(env, js_object_name));
}

std::vector<ScopedJavaLocalRef<jobject>> AwContents::GetWebMessageListenerInfos(
    JNIEnv* env) {
  if (js_communication_host_.get()) {
    return AwWebMessageHostFactory::GetWebMessageListenerInfo(
        GetJsCommunicationHost(), env);
  }
  return {};
}

std::vector<ScopedJavaLocalRef<jobject>>
AwContents::GetDocumentStartupJavascripts(JNIEnv* env) {
  if (!js_communication_host_.get()) {
    return {};
  }

  const std::vector<js_injection::DocumentStartJavaScript>& scripts =
      GetJsCommunicationHost()->GetDocumentStartJavascripts();

  std::vector<ScopedJavaLocalRef<jobject>> script_objects;
  for (const auto& script : scripts) {
    const std::vector<std::string> rules =
        script.allowed_origin_rules_.Serialize();
    script_objects.push_back(Java_StartupJavascriptInfo_create(
        env, base::android::ConvertUTF16ToJavaString(env, script.script_),
        base::android::ToJavaArrayOfStrings(env, rules)));
  }

  return script_objects;
}

void AwContents::FlushBackForwardCache(JNIEnv* env, jint reason) {
  web_contents()->GetController().GetBackForwardCache().Flush(
      static_cast<NotRestoredReason>(reason));
}

void AwContents::CancelAllPrerendering(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents()->CancelAllPrerendering();
}

void AwContents::ClearView(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_view_renderer_.ClearView();
}

void AwContents::SetExtraHeadersForUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& jextra_headers) {
  std::string extra_headers;
  if (jextra_headers)
    extra_headers = ConvertJavaStringToUTF8(env, jextra_headers);
  auto* browser_context =
      AwBrowserContext::FromWebContents(web_contents_.get());
  browser_context->SetExtraHeaders(GURL(ConvertJavaStringToUTF8(env, url)),
                                   extra_headers);
}

void AwContents::SetJsOnlineProperty(JNIEnv* env, jboolean network_up) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AwRenderProcess* aw_render_process =
      AwRenderProcess::GetInstanceForRenderProcessHost(
          web_contents_->GetPrimaryMainFrame()->GetProcess());

  aw_render_process->SetJsOnlineProperty(network_up);
}

void AwContents::TrimMemory(JNIEnv* env, jint level, jboolean visible) {
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

void AwContents::GrantFileSchemeAccesstoChildProcess(JNIEnv* env) {
  content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
      web_contents_->GetPrimaryMainFrame()->GetProcess()->GetID(),
      url::kFileScheme);
}

void AwContents::ResumeLoadingCreatedPopupWebContents(JNIEnv* env) {
  web_contents_->ResumeLoadingCreatedWebContents();
}

void JNI_AwContents_SetShouldDownloadFavicons(JNIEnv* env) {
  g_should_download_favicons = true;
}

namespace {

// Returns true if any of the `domains` match the `etld_plus1`.
bool IncludesETLDPlusOne(const std::string& etld_plus1,
                         const std::vector<std::string>& domains) {
  return std::find_if(
             domains.begin(), domains.end(), [&](const std::string& domain) {
               return etld_plus1 ==
                      net::registry_controlled_domains::GetDomainAndRegistry(
                          domain, net::registry_controlled_domains::
                                      INCLUDE_PRIVATE_REGISTRIES);
             }) != domains.end();
}

// Post a task to a background thread to log a site visit.
void LogSiteVisitOnBackgroundThread(jlong site_hash, bool is_related_site) {
  // Logging a site visit involves writing to shared preferences, which should
  // not be done on the main thread.
  base::ThreadPool::PostTask(FROM_HERE,
                             base::BindOnce(
                                 [](jlong site_hash, bool is_related_site) {
                                   JNIEnv* env = AttachCurrentThread();
                                   Java_AwSiteVisitLogger_logVisit(
                                       env, site_hash, is_related_site);
                                 },
                                 site_hash, is_related_site));
}
}  // namespace

void LogSiteVisit(std::string etld_plus1, jlong site_hash) {
  AppDefinedWebsites::GetInstance()->GetAppDefinedDomains(
      AppDefinedDomainCriteria::kAndroidAssetStatementsAndWebLinks,
      base::BindOnce(
          [](std::string etld_plus1, jlong site_hash,
             const std::vector<std::string>& domains) {
            LogSiteVisitOnBackgroundThread(
                site_hash, IncludesETLDPlusOne(etld_plus1, domains));
          },
          std::move(etld_plus1), site_hash));
}

void AwContents::PrimaryPageChanged(content::Page& page) {
  std::string scheme = page.GetMainDocument().GetLastCommittedURL().scheme();
  const url::Origin& origin = page.GetMainDocument().GetLastCommittedOrigin();
  std::string etld_plus1 =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (scheme_ != scheme) {
    scheme_ = scheme;
    AwBrowserProcess::GetInstance()
        ->visibility_metrics_logger()
        ->ClientVisibilityChanged(this);
  }

  if (scheme == url::kHttpsScheme || scheme == url::kHttpScheme) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> j_ref = java_ref_.get(env);
    if (j_ref) {
      uint32_t origin_hash = base::PersistentHash(origin.Serialize());
      uint32_t etld_plus1_hash = base::PersistentHash(etld_plus1);

      jlong j_origin_hash = static_cast<jlong>(origin_hash);
      jlong j_etld_plus1_hash = static_cast<jlong>(etld_plus1_hash);

      Java_AwContents_logOriginVisit(env, j_ref, j_origin_hash);

      LogSiteVisit(std::move(etld_plus1), j_etld_plus1_hash);
    }
  }

  // At this point, the current RenderFrameHost may or may not contain a
  // compositor. So compositor_ may be nullptr, in which case
  // BrowserViewRenderer::DidInitializeCompositor() callback is time when the
  // new compositor is constructed.
  browser_view_renderer_.SetActiveFrameSinkId(
      page.GetMainDocument().GetRenderWidgetHost()->GetFrameSinkId());
}

void AwContents::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If this request was blocked in any way, broadcast an error.
  net::Error error_code = navigation_handle->GetNetErrorCode();
  if (!net::IsRequestBlockedError(error_code) &&
      error_code != net::ERR_ABORTED) {
    return;
  }

  // We do not call OnReceivedError for requests that were blocked due to an
  // interstitial showing. OnReceivedError is handled directly by the blocking
  // page for interstitials.
  if (web_contents_) {
    // We can't be showing an interstitial if there is no web_contents.
    security_interstitials::SecurityInterstitialTabHelper*
        security_interstitial_tab_helper = security_interstitials::
            SecurityInterstitialTabHelper::FromWebContents(web_contents_.get());
    if (security_interstitial_tab_helper &&
        (security_interstitial_tab_helper->IsInterstitialPendingForNavigation(
             navigation_handle->GetNavigationId()) ||
         security_interstitial_tab_helper->IsDisplayingInterstitial())) {
      return;
    }
  }

  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents_.get());
  if (!client)
    return;

  AwWebResourceRequest request(navigation_handle->GetURL().spec(),
                               navigation_handle->IsPost() ? "POST" : "GET",
                               navigation_handle->IsInPrimaryMainFrame(),
                               navigation_handle->HasUserGesture(),
                               net::HttpRequestHeaders());
  request.is_renderer_initiated = navigation_handle->IsRendererInitiated();
  client->OnReceivedError(request, error_code, false, false);
}

void AwContents::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // In Android WebView, mixed content auto-upgrade is determined by an
  // AwSetting. The result is computed and stored on WebPreferences. However on
  // other platforms this setting is determined on a per-navigation basis. Thus,
  // we need to propagate this information to the navigation.
  auto content_settings = blink::CreateDefaultRendererContentSettings();
  content_settings->allow_mixed_content = navigation_handle->GetWebContents()
                                              ->GetOrCreateWebPreferences()
                                              .allow_mixed_content_upgrades;
  navigation_handle->SetContentSettings(std::move(content_settings));
}

void AwContents::RenderViewReady() {
  AwRenderProcess::SetRenderViewReady(
      web_contents_->GetPrimaryMainFrame()->GetProcess());
}

bool AwContents::CanShowInterstitial() {
  JNIEnv* env = AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return false;
  return Java_AwContents_canShowInterstitial(env, obj);
}

int AwContents::GetErrorUiType() {
  JNIEnv* env = AttachCurrentThread();
  const ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return false;
  return Java_AwContents_getErrorUiType(env, obj);
}

VisibilityMetricsLogger::VisibilityInfo AwContents::GetVisibilityInfo() {
  return VisibilityMetricsLogger::VisibilityInfo{
      browser_view_renderer_.attached_to_window(),
      browser_view_renderer_.view_visible(),
      browser_view_renderer_.window_visible(),
      VisibilityMetricsLogger::SchemeStringToEnum(scheme_)};
}

void AwContents::RendererUnresponsive(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
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
  if (!obj)
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
  if (!obj)
    return RenderProcessGoneResult::kHandled;

  bool result =
      Java_AwContents_onRenderProcessGone(env, obj, child_process_id, crashed);

  if (HasException(env))
    return RenderProcessGoneResult::kException;

  return result ? RenderProcessGoneResult::kHandled
                : RenderProcessGoneResult::kUnhandled;
}

void AwContents::OnSafeBrowsingAllowListSet() {
  web_contents()->GetController().GetBackForwardCache().Flush(
      NotRestoredReason::kWebViewSafeBrowsingAllowlistChanged);
}

}  // namespace android_webview
