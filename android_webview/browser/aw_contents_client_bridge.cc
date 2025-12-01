// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_client_bridge.h"

#include <memory>
#include <optional>
#include <string>
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
      return nullptr;
    UserData* data = static_cast<UserData*>(
        web_contents->GetUserData(kAwContentsClientBridge));
    return data ? data->contents_.get() : nullptr;
  }

  explicit UserData(AwContentsClientBridge* ptr) : contents_(ptr) {}

  UserData(const UserData&) = delete;
  UserData& operator=(const UserData&) = delete;

 private:
  raw_ptr<AwContentsClientBridge> contents_;
};

}  // namespace

AwContentsClientBridge::HttpErrorInfo::HttpErrorInfo() : status_code(0) {}

AwContentsClientBridge::HttpErrorInfo::~HttpErrorInfo() = default;

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
  // We need to add the callback before making the call to java side,
  // as it may do a synchronous callback prior to returning.
  int request_id = pending_cert_error_callbacks_.Add(
      std::make_unique<CertErrorCallback>(std::move(callback)));
  *cancel_request = !Java_AwContentsClientBridge_allowCertificateError(
      env, obj, cert_error, jcert, request_url.spec(), request_id);
  // if the request is cancelled, then cancel the stored callback
  if (*cancel_request) {
    pending_cert_error_callbacks_.Remove(request_id);
  }
}

void AwContentsClientBridge::ProceedSslError(JNIEnv* env,
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

  // Build the |encoded_principals| JNI parameter, as a byte[][]
  ScopedJavaLocalRef<jobjectArray> principals_ref =
      base::android::ToJavaArrayOfByteArray(
          env, cert_request_info->cert_authorities);
  if (!principals_ref) {
    LOG(ERROR) << "Could not create principals array (byte[][])";
    return;
  }

  int request_id =
      pending_client_cert_request_delegates_.Add(std::move(delegate));
  Java_AwContentsClientBridge_selectClientCertificate(
      env, obj, request_id, key_types, principals_ref,
      cert_request_info->host_and_port.host(),
      cert_request_info->host_and_port.port());
}

// This method is inspired by OnSystemRequestCompletion() in
// components/browser_ui/client_certificate/android/
// ssl_client_certificate_request.cc
void AwContentsClientBridge::ProvideClientCertificateResponse(
    JNIEnv* env,
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
  for (const auto& encoded_chain_string : encoded_chain_strings) {
    encoded_chain.push_back(encoded_chain_string);
  }

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
  switch (dialog_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT: {
      devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
          "onJsAlert");
      Java_AwContentsClientBridge_handleJsAlert(env, obj, origin_url.spec(),
                                                message_text, callback_id);
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
      devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
          "onJsConfirm");
      Java_AwContentsClientBridge_handleJsConfirm(env, obj, origin_url.spec(),
                                                  message_text, callback_id);
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
      devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
          "onJsPrompt");
      Java_AwContentsClientBridge_handleJsPrompt(
          env, obj, origin_url.spec(), message_text, default_prompt_text,
          callback_id);
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

  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "onJsBeforeUnload");
  Java_AwContentsClientBridge_handleJsBeforeUnload(env, obj, origin_url.spec(),
                                                   message_text, callback_id);
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
  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "shouldOverrideUrlLoading");

  std::vector<std::string> header_names;
  std::vector<std::string> header_values;
  ConvertRequestHeadersToVectors(request_headers, &header_names,
                                 &header_values);

  *ignore_navigation = Java_AwContentsClientBridge_shouldOverrideUrlLoading(
      env, obj, url, has_user_gesture, is_redirect, header_names, header_values,
      is_outermost_main_frame);
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
  return Java_AwContentsClientBridge_sendBrowseIntent(env, obj, url);
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

  Java_AwContentsClientBridge_newDownload(env, obj, url.spec(), user_agent,
                                          content_disposition, mime_type,
                                          content_length);
}

void AwContentsClientBridge::NewLoginRequest(const std::string& realm,
                                             const std::string& account,
                                             const std::string& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;
  // The API expects nullptr rather than empty string if account is missing.
  const std::string* account_or_null = account.empty() ? nullptr : &account;
  Java_AwContentsClientBridge_newLoginRequest(env, obj, realm, account_or_null,
                                              args);
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

  Java_AwContentsClientBridge_onReceivedError(
      env, obj, request, request.is_renderer_initiated.value_or(false),
      error_code, net::ErrorToString(error_code), safebrowsing_hit,
      should_omit_notifications_for_safebrowsing_hit);
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

  Java_AwContentsClientBridge_onSafeBrowsingHit(
      env, obj, request, static_cast<int>(threat_type), request_id);
}

void AwContentsClientBridge::OnReceivedHttpError(
    const AwWebResourceRequest& request,
    std::unique_ptr<HttpErrorInfo> http_error_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj)
    return;

  Java_AwContentsClientBridge_onReceivedHttpError(
      env, obj, request, http_error_info->mime_type, http_error_info->encoding,
      http_error_info->status_code, http_error_info->status_text,
      http_error_info->response_header_names,
      http_error_info->response_header_values);
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

void AwContentsClientBridge::ConfirmJsResult(
    JNIEnv* env,
    int id,
    std::optional<std::u16string> prompt) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::JavaScriptDialogManager::DialogClosedCallback* callback =
      pending_js_dialog_callbacks_.Lookup(id);
  if (!callback) {
    LOG(WARNING) << "Unexpected JS dialog confirm. " << id;
    return;
  }
  std::move(*callback).Run(true, prompt.value_or(std::u16string()));
  pending_js_dialog_callbacks_.Remove(id);
}

void AwContentsClientBridge::TakeSafeBrowsingAction(JNIEnv*,
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

void AwContentsClientBridge::CancelJsResult(JNIEnv*, int id) {
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

DEFINE_JNI(AwContentsClientBridge)
