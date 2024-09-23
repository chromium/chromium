// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_client_bridge.h"

#include <memory>
#include <string_view>
#include <utility>

#include "android_webview/browser/network_service/net_helpers.h"
#include "android_webview/common/devtools_instrumentation.h"
#include "android_webview/grit/components_strings.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/current_thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_platform_key_android.h"
#include "net/ssl/ssl_private_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwContentsClientBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::HasException;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::BrowserThread;
using content::WebContents;
using std::vector;

namespace android_webview {

namespace {

const void* const kAwContentsClientBridge = &kAwContentsClientBridge;

// This class is invented so that the UserData registry that we inject the
// AwContentsClientBridge object does not own and destroy it.
class UserData : public base::SupportsUserData::Data {
 public:
  static AwContentsClientBridge* GetContents(
      content::WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    UserData* data = static_cast<UserData*>(
        web_contents->GetUserData(kAwContentsClientBridge));
    return data ? data->contents_.get() : NULL;
  }

  explicit UserData(AwContentsClientBridge* ptr) : contents_(ptr) {}

  UserData(const UserData&) = delete;
  UserData& operator=(const UserData&) = delete;

 private:
  raw_ptr<AwContentsClientBridge> contents_;
};

}  // namespace

AwContentsClientBridge::HttpErrorInfo::HttpErrorInfo() : status_code(0) {}

AwContentsClientBridge::HttpErrorInfo::~HttpErrorInfo() {}

// static
void AwContentsClientBridge::Associate(WebContents* web_contents,
                                       AwContentsClientBridge* handler) {
  web_contents->SetUserData(kAwContentsClientBridge,
                            std::make_unique<UserData>(handler));
}

// static
void AwContentsClientBridge::Dissociate(WebContents* web_contents) {
  web_contents->RemoveUserData(kAwContentsClientBridge);
}

// static
AwContentsClientBridge* AwContentsClientBridge::FromWebContents(
    WebContents* web_contents) {
  return UserData::GetContents(web_contents);
}

AwContentsClientBridge::AwContentsClientBridge(JNIEnv* env,
                                               const JavaRef<jobject>& obj)
    : java_ref_(env, obj) {
  DCHECK(obj);
  Java_AwContentsClientBridge_setNativeContentsClientBridge(
      env, obj, reinterpret_cast<intptr_t>(this));
}

AwContentsClientBridge::~AwContentsClientBridge() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj) {
    // Clear the weak reference from the java peer to the native object since
    // it is possible that java object lifetime can exceed the AwContens.
    Java_AwContentsClientBridge_setNativeContentsClientBridge(env, obj, 0);
  }
}

void AwContentsClientBridge::AllowCertificateError(int cert_error,
                                                   net::X509Certificate* cert,
                                                   const GURL& request_url,
                                                   CertErrorCallback callback,
                                                   bool* cancel_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  std::string_view der_string =
      net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer());
  ScopedJavaLocalRef<jbyteArray> jcert =
      base::android::ToJavaByteArray(env, base::as_byte_span(der_string));
  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, request_url.spec()));
  // We need to add the callback before making the call to java side,
  // as it may do a synchronous callback prior to returning.
  int request_id = pending_cert_error_callbacks_.Add(
      std::make_unique<CertErrorCallback>(std::move(callback)));
  *cancel_request = !Java_AwContentsClientBridge_allowCertificateError(
      env, obj, cert_error, jcert, jurl, request_id);
  // if the request is cancelled, then cancel the stored callback
  if (*cancel_request) {
    pending_cert_error_callbacks_.Remove(request_id);
  }
}

void AwContentsClientBridge::ProceedSslError(JNIEnv* env,
                                             const JavaRef<jobject>& obj,
                                             jboolean proceed,
                                             jint id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CertErrorCallback* callback = pending_cert_error_callbacks_.Lookup(id);
  if (!callback || callback->is_null()) {
    LOG(WARNING) << "Ignoring unexpected ssl error proceed callback";
    return;
  }
  std::move(*callback).Run(
      proceed ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE
              : content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  pending_cert_error_callbacks_.Remove(id);
}

// This method is inspired by SelectClientCertificate() in
// components/browser_ui/client_certificate/android/
// ssl_client_certificate_request.cc
void AwContentsClientBridge::SelectClientCertificate(
    net::SSLCertRequestInfo* cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  // Build the |key_types| JNI parameter, as a String[]
  std::vector<std::string> key_types = net::SignatureAlgorithmsToJavaKeyTypes(
      cert_request_info->signature_algorithms);
  ScopedJavaLocalRef<jobjectArray> key_types_ref =
      base::android::ToJavaArrayOfStrings(env, key_types);
  if (!key_types_ref) {
    LOG(ERROR) << "Could not create key types array (String[])";
    return;
  }

  // Build the |encoded_principals| JNI parameter, as a byte[][]
  ScopedJavaLocalRef<jobjectArray> principals_ref =
      base::android::ToJavaArrayOfByteArray(
          env, cert_request_info->cert_authorities);
  if (!principals_ref) {
    LOG(ERROR) << "Could not create principals array (byte[][])";
    return;
  }

  // Build the |host_name| and |port| JNI parameters, as a String and
  // a jint.
  ScopedJavaLocalRef<jstring> host_name_ref =
      base::android::ConvertUTF8ToJavaString(
          env, cert_request_info->host_and_port.host());

  int request_id =
      pending_client_cert_request_delegates_.Add(std::move(delegate));
  Java_AwContentsClientBridge_selectClientCertificate(
      env, obj, request_id, key_types_ref, principals_ref, host_name_ref,
      cert_request_info->host_and_port.port());
}

// This method is inspired by OnSystemRequestCompletion() in
// components/browser_ui/client_certificate/android/
// ssl_client_certificate_request.cc
void AwContentsClientBridge::ProvideClientCertificateResponse(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    int request_id,
    const JavaRef<jobjectArray>& encoded_chain_ref,
    const JavaRef<jobject>& private_key_ref) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<content::ClientCertificateDelegate> delegate =
      pending_client_cert_request_delegates_.Replace(request_id, nullptr);
  pending_client_cert_request_delegates_.Remove(request_id);
  DCHECK(delegate);

  if (!encoded_chain_ref || !private_key_ref) {
    LOG(ERROR) << "No client certificate selected";
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return;
  }

  // Convert the encoded chain to a vector of strings.
  std::vector<std::string> encoded_chain_strings;
  if (encoded_chain_ref) {
    base::android::JavaArrayOfByteArrayToStringVector(env, encoded_chain_ref,
                                                      &encoded_chain_strings);
  }

  std::vector<std::string_view> encoded_chain;
  for (size_t i = 0; i < encoded_chain_strings.size(); ++i)
    encoded_chain.push_back(encoded_chain_strings[i]);

  // Create the X509Certificate object from the encoded chain.
  scoped_refptr<net::X509Certificate> client_cert(
      net::X509Certificate::CreateFromDERCertChain(encoded_chain));
  if (!client_cert.get()) {
    LOG(ERROR) << "Could not decode client certificate chain";
    return;
  }

  // Create an SSLPrivateKey wrapper for the private key JNI reference.
  scoped_refptr<net::SSLPrivateKey> private_key =
      net::WrapJavaPrivateKey(client_cert.get(), private_key_ref);
  if (!private_key) {
    LOG(ERROR) << "Could not create OpenSSL wrapper for private key";
    return;
  }

  delegate->ContinueWithCertificate(std::move(client_cert),
                                    std::move(private_key));
}

void AwContentsClientBridge::RunJavaScriptDialog(
    content::JavaScriptDialogType dialog_type,
    const GURL& origin_url,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj) {
    std::move(callback).Run(false, std::u16string());
    return;
  }

  int callback_id = pending_js_dialog_callbacks_.Add(
      std::make_unique<content::JavaScriptDialogManager::DialogClosedCallback>(
          std::move(callback)));
  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, origin_url.spec()));
  ScopedJavaLocalRef<jstring> jmessage(
      ConvertUTF16ToJavaString(env, message_text));

  switch (dialog_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT: {
      devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
          "onJsAlert");
      Java_AwContentsClientBridge_handleJsAlert(env, obj, jurl, jmessage,
                                                callback_id);
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
      devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
          "onJsConfirm");
      Java_AwContentsClientBridge_handleJsConfirm(env, obj, jurl, jmessage,
                                                  callback_id);
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
      ScopedJavaLocalRef<jstring> jdefault_value(
          ConvertUTF16ToJavaString(env, default_prompt_text));
      devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
          "onJsPrompt");
      Java_AwContentsClientBridge_handleJsPrompt(env, obj, jurl, jmessage,
                                                 jdefault_value, callback_id);
      break;
    }
    default:
      NOTREACHED();
  }
}

void AwContentsClientBridge::RunBeforeUnloadDialog(
    const GURL& origin_url,
    content::JavaScriptDialogManager::DialogClosedCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj) {
    std::move(callback).Run(false, std::u16string());
    return;
  }

  const std::u16string message_text =
      l10n_util::GetStringUTF16(IDS_BEFOREUNLOAD_MESSAGEBOX_MESSAGE);

  int callback_id = pending_js_dialog_callbacks_.Add(
      std::make_unique<content::JavaScriptDialogManager::DialogClosedCallback>(
          std::move(callback)));
  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, origin_url.spec()));
  ScopedJavaLocalRef<jstring> jmessage(
      ConvertUTF16ToJavaString(env, message_text));

  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "onJsBeforeUnload");
  Java_AwContentsClientBridge_handleJsBeforeUnload(env, obj, jurl, jmessage,
                                                   callback_id);
}

bool AwContentsClientBridge::ShouldOverrideUrlLoading(
    const std::u16string& url,
    bool has_user_gesture,
    bool is_redirect,
    bool is_outermost_main_frame,
    const net::HttpRequestHeaders& request_headers,
    bool* ignore_navigation) {
  *ignore_navigation = false;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return true;
  ScopedJavaLocalRef<jstring> jurl = ConvertUTF16ToJavaString(env, url);
  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "shouldOverrideUrlLoading");

  std::vector<std::string> header_names;
  std::vector<std::string> header_values;
  ConvertRequestHeadersToVectors(request_headers, &header_names,
                                 &header_values);

  ScopedJavaLocalRef<jobjectArray> jheader_names =
      ToJavaArrayOfStrings(env, header_names);
  ScopedJavaLocalRef<jobjectArray> jheader_values =
      ToJavaArrayOfStrings(env, header_values);

  *ignore_navigation = Java_AwContentsClientBridge_shouldOverrideUrlLoading(
      env, obj, jurl, has_user_gesture, is_redirect, jheader_names,
      jheader_values, is_outermost_main_frame);
  if (HasException(env)) {
    // Tell the chromium message loop to not perform any tasks after the current
    // one - we want to make sure we return to Java cleanly without first making
    // any new JNI calls.
    base::CurrentUIThread::Get()->Abort();
    // If we crashed we don't want to continue the navigation.
    *ignore_navigation = true;
    return false;
  }
  return true;
}

bool AwContentsClientBridge::SendBrowseIntent(const std::u16string& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return false;
  ScopedJavaLocalRef<jstring> jurl = ConvertUTF16ToJavaString(env, url);
  return Java_AwContentsClientBridge_sendBrowseIntent(env, obj, jurl);
}

void AwContentsClientBridge::NewDownload(const GURL& url,
                                         const std::string& user_agent,
                                         const std::string& content_disposition,
                                         const std::string& mime_type,
                                         int64_t content_length) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> jstring_user_agent =
      ConvertUTF8ToJavaString(env, user_agent);
  ScopedJavaLocalRef<jstring> jstring_content_disposition =
      ConvertUTF8ToJavaString(env, content_disposition);
  ScopedJavaLocalRef<jstring> jstring_mime_type =
      ConvertUTF8ToJavaString(env, mime_type);

  Java_AwContentsClientBridge_newDownload(
      env, obj, jstring_url, jstring_user_agent, jstring_content_disposition,
      jstring_mime_type, content_length);
}

void AwContentsClientBridge::NewLoginRequest(const std::string& realm,
                                             const std::string& account,
                                             const std::string& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  ScopedJavaLocalRef<jstring> jrealm = ConvertUTF8ToJavaString(env, realm);
  ScopedJavaLocalRef<jstring> jargs = ConvertUTF8ToJavaString(env, args);

  ScopedJavaLocalRef<jstring> jaccount;
  if (!account.empty())
    jaccount = ConvertUTF8ToJavaString(env, account);

  Java_AwContentsClientBridge_newLoginRequest(env, obj, jrealm, jaccount,
                                              jargs);
}

void AwContentsClientBridge::OnReceivedError(
    const AwWebResourceRequest& request,
    int error_code,
    bool safebrowsing_hit,
    bool should_omit_notifications_for_safebrowsing_hit) {
  DCHECK(request.is_renderer_initiated.has_value());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  ScopedJavaLocalRef<jstring> jstring_description =
      ConvertUTF8ToJavaString(env, net::ErrorToString(error_code));

  AwWebResourceRequest::AwJavaWebResourceRequest java_web_resource_request;
  AwWebResourceRequest::ConvertToJava(env, request, &java_web_resource_request);
  Java_AwContentsClientBridge_onReceivedError(
      env, obj, java_web_resource_request.jurl, request.is_outermost_main_frame,
      request.has_user_gesture, *request.is_renderer_initiated,
      java_web_resource_request.jmethod,
      java_web_resource_request.jheader_names,
      java_web_resource_request.jheader_values, error_code, jstring_description,
      safebrowsing_hit, should_omit_notifications_for_safebrowsing_hit);
}

void AwContentsClientBridge::OnSafeBrowsingHit(
    const AwWebResourceRequest& request,
    const safe_browsing::SBThreatType& threat_type,
    SafeBrowsingActionCallback callback) {
  int request_id = safe_browsing_callbacks_.Add(
      std::make_unique<SafeBrowsingActionCallback>(std::move(callback)));

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  AwWebResourceRequest::AwJavaWebResourceRequest java_web_resource_request;
  AwWebResourceRequest::ConvertToJava(env, request, &java_web_resource_request);
  Java_AwContentsClientBridge_onSafeBrowsingHit(
      env, obj, java_web_resource_request.jurl, request.is_outermost_main_frame,
      request.has_user_gesture, java_web_resource_request.jmethod,
      java_web_resource_request.jheader_names,
      java_web_resource_request.jheader_values, static_cast<int>(threat_type),
      request_id);
}

void AwContentsClientBridge::OnReceivedHttpError(
    const AwWebResourceRequest& request,
    std::unique_ptr<HttpErrorInfo> http_error_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  AwWebResourceRequest::AwJavaWebResourceRequest java_web_resource_request;
  AwWebResourceRequest::ConvertToJava(env, request, &java_web_resource_request);

  ScopedJavaLocalRef<jstring> jstring_mime_type =
      ConvertUTF8ToJavaString(env, http_error_info->mime_type);
  ScopedJavaLocalRef<jstring> jstring_encoding =
      ConvertUTF8ToJavaString(env, http_error_info->encoding);
  ScopedJavaLocalRef<jstring> jstring_reason =
      ConvertUTF8ToJavaString(env, http_error_info->status_text);
  ScopedJavaLocalRef<jobjectArray> jstringArray_response_header_names =
      ToJavaArrayOfStrings(env, http_error_info->response_header_names);
  ScopedJavaLocalRef<jobjectArray> jstringArray_response_header_values =
      ToJavaArrayOfStrings(env, http_error_info->response_header_values);

  Java_AwContentsClientBridge_onReceivedHttpError(
      env, obj, java_web_resource_request.jurl, request.is_outermost_main_frame,
      request.has_user_gesture, java_web_resource_request.jmethod,
      java_web_resource_request.jheader_names,
      java_web_resource_request.jheader_values, jstring_mime_type,
      jstring_encoding, http_error_info->status_code, jstring_reason,
      jstringArray_response_header_names, jstringArray_response_header_values);
}

// static
std::unique_ptr<AwContentsClientBridge::HttpErrorInfo>
AwContentsClientBridge::ExtractHttpErrorInfo(
    const net::HttpResponseHeaders* response_headers) {
  auto http_error_info = std::make_unique<HttpErrorInfo>();
  {
    size_t headers_iterator = 0;
    std::string header_name, header_value;
    while (response_headers->EnumerateHeaderLines(
        &headers_iterator, &header_name, &header_value)) {
      http_error_info->response_header_names.push_back(header_name);
      http_error_info->response_header_values.push_back(header_value);
    }
  }

  response_headers->GetMimeTypeAndCharset(&http_error_info->mime_type,
                                          &http_error_info->encoding);
  http_error_info->status_code = response_headers->response_code();
  http_error_info->status_text = response_headers->GetStatusText();
  return http_error_info;
}

void AwContentsClientBridge::ConfirmJsResult(JNIEnv* env,
                                             const JavaRef<jobject>&,
                                             int id,
                                             const JavaRef<jstring>& prompt) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::JavaScriptDialogManager::DialogClosedCallback* callback =
      pending_js_dialog_callbacks_.Lookup(id);
  if (!callback) {
    LOG(WARNING) << "Unexpected JS dialog confirm. " << id;
    return;
  }
  std::u16string prompt_text;
  if (prompt) {
    prompt_text = ConvertJavaStringToUTF16(env, prompt);
  }
  std::move(*callback).Run(true, prompt_text);
  pending_js_dialog_callbacks_.Remove(id);
}

void AwContentsClientBridge::TakeSafeBrowsingAction(JNIEnv*,
                                                    const JavaRef<jobject>&,
                                                    int action,
                                                    bool reporting,
                                                    int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* callback = safe_browsing_callbacks_.Lookup(request_id);
  if (!callback) {
    LOG(WARNING) << "Unexpected TakeSafeBrowsingAction. " << request_id;
    return;
  }
  std::move(*callback).Run(
      static_cast<AwUrlCheckerDelegateImpl::SafeBrowsingAction>(action),
      reporting);
  safe_browsing_callbacks_.Remove(request_id);
}

void AwContentsClientBridge::CancelJsResult(JNIEnv*,
                                            const JavaRef<jobject>&,
                                            int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::JavaScriptDialogManager::DialogClosedCallback* callback =
      pending_js_dialog_callbacks_.Lookup(id);
  if (!callback) {
    LOG(WARNING) << "Unexpected JS dialog cancel. " << id;
    return;
  }
  std::move(*callback).Run(false, std::u16string());
  pending_js_dialog_callbacks_.Remove(id);
}

}  // namespace android_webview
