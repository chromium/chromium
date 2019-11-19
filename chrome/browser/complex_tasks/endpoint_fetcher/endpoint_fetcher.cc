// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/complex_tasks/endpoint_fetcher/endpoint_fetcher.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if defined(OS_ANDROID)

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/EndpointFetcher_jni.h"
#include "chrome/android/chrome_jni_headers/EndpointResponse_jni.h"
#include "chrome/browser/profiles/profile_android.h"

#endif  // defined(OS_ANDROID)

namespace {
const char kContentTypeKey[] = "Content-Type";
const char kDeveloperKey[] = "X-Developer-Key";
const int kNumRetries = 3;
}  // namespace

EndpointFetcher::EndpointFetcher(
    Profile* const profile,
    const std::string& oauth_consumer_name,
    const GURL& url,
    const std::string& http_method,
    const std::string& content_type,
    const std::vector<std::string>& scopes,
    int64_t timeout_ms,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag)
    : EndpointFetcher(
          oauth_consumer_name,
          url,
          http_method,
          content_type,
          scopes,
          timeout_ms,
          post_data,
          annotation_tag,
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetURLLoaderFactoryForBrowserProcess(),
          IdentityManagerFactory::GetForProfile(profile)) {}

EndpointFetcher::EndpointFetcher(
    const std::string& oauth_consumer_name,
    const GURL& url,
    const std::string& http_method,
    const std::string& content_type,
    const std::vector<std::string>& scopes,
    int64_t timeout_ms,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    signin::IdentityManager* const identity_manager)
    : oauth_consumer_name_(oauth_consumer_name),
      url_(url),
      http_method_(http_method),
      content_type_(content_type),
      timeout_ms_(timeout_ms),
      post_data_(post_data),
      annotation_tag_(annotation_tag),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {
  for (auto scope : scopes) {
    oauth_scopes_.insert(scope);
  }
}

EndpointFetcher::~EndpointFetcher() = default;

void EndpointFetcher::Fetch(EndpointFetcherCallback endpoint_fetcher_callback) {
  signin::AccessTokenFetcher::TokenCallback token_callback = base::BindOnce(
      &EndpointFetcher::OnAuthTokenFetched, weak_ptr_factory_.GetWeakPtr(),
      std::move(endpoint_fetcher_callback));
  DCHECK(!access_token_fetcher_);
  DCHECK(!simple_url_loader_);
  // TODO(crbug.com/997018) Make access_token_fetcher_ local variable passed to
  // callback
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          oauth_consumer_name_, identity_manager_, oauth_scopes_,
          std::move(token_callback),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void EndpointFetcher::OnAuthTokenFetched(
    EndpointFetcherCallback endpoint_fetcher_callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    auto response = std::make_unique<EndpointResponse>();
    response->response = "There was an authentication error";
    // TODO(crbug.com/993393) Add more detailed error messaging
    std::move(endpoint_fetcher_callback).Run(std::move(response));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = http_method_;
  resource_request->url = url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (base::EqualsCaseInsensitiveASCII(http_method_, "POST")) {
    resource_request->headers.SetHeader(kContentTypeKey, content_type_);
  }
  resource_request->headers.SetHeader(
      kDeveloperKey, GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf("Bearer %s", access_token_info.token.c_str()));

  // TODO(crbug.com/997018) Make simple_url_loader_ local variable passed to
  // callback
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), annotation_tag_);

  if (base::EqualsCaseInsensitiveASCII(http_method_, "POST")) {
    simple_url_loader_->AttachStringForUpload(post_data_, content_type_);
  }
  simple_url_loader_->SetRetryOptions(kNumRetries,
                                      network::SimpleURLLoader::RETRY_ON_5XX);
  simple_url_loader_->SetTimeoutDuration(
      base::TimeDelta::FromMilliseconds(timeout_ms_));

  network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback =
      base::BindOnce(&EndpointFetcher::OnResponseFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(endpoint_fetcher_callback));
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(), std::move(body_as_string_callback),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void EndpointFetcher::OnResponseFetched(
    EndpointFetcherCallback endpoint_fetcher_callback,
    std::unique_ptr<std::string> response_body) {
  simple_url_loader_.reset();
  auto response = std::make_unique<EndpointResponse>();
  // TODO(crbug.com/993393) Add more detailed error messaging
  response->response =
      response_body ? std::move(*response_body) : "There was a response error";
  std::move(endpoint_fetcher_callback).Run(std::move(response));
}

#if defined(OS_ANDROID)
namespace {
static void OnEndpointFetcherComplete(
    const base::android::JavaRef<jobject>& jcaller,
    // Passing the endpoint_fetcher ensures the endpoint_fetcher's
    // lifetime extends to the callback and is not destroyed
    // prematurely (which would result in cancellation of the request).
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> endpoint_response) {
  base::android::RunObjectCallbackAndroid(
      jcaller, Java_EndpointResponse_createEndpointResponse(
                   base::android::AttachCurrentThread(),
                   base::android::ConvertUTF8ToJavaString(
                       base::android::AttachCurrentThread(),
                       std::move(endpoint_response->response))));
}
}  // namespace

static void JNI_EndpointFetcher_NativeFetch(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile,
    const base::android::JavaParamRef<jstring>& joauth_consumer_name,
    const base::android::JavaParamRef<jstring>& jurl,
    const base::android::JavaParamRef<jstring>& jhttps_method,
    const base::android::JavaParamRef<jstring>& jcontent_type,
    const base::android::JavaParamRef<jobjectArray>& jscopes,
    const base::android::JavaParamRef<jstring>& jpost_data,
    jlong jtimeout,
    const base::android::JavaParamRef<jobject>& jcallback) {
  std::vector<std::string> scopes;
  base::android::AppendJavaStringArrayToStringVector(env, jscopes, &scopes);
  auto endpoint_fetcher = std::make_unique<EndpointFetcher>(
      ProfileAndroid::FromProfileAndroid(jprofile),
      base::android::ConvertJavaStringToUTF8(env, joauth_consumer_name),
      GURL(base::android::ConvertJavaStringToUTF8(env, jurl)),
      base::android::ConvertJavaStringToUTF8(env, jhttps_method),
      base::android::ConvertJavaStringToUTF8(env, jcontent_type), scopes,
      jtimeout, base::android::ConvertJavaStringToUTF8(env, jpost_data),
      // TODO(crbug.com/995852) Create a traffic annotation tag and configure it
      // as part of the EndpointFetcher call over JNI.
      NO_TRAFFIC_ANNOTATION_YET);
  endpoint_fetcher->Fetch(
      base::BindOnce(&OnEndpointFetcherComplete,
                     base::android::ScopedJavaGlobalRef<jobject>(jcallback),
                     // unique_ptr endpoint_fetcher is passed until the callback
                     // to ensure its lifetime across the request.
                     std::move(endpoint_fetcher)));
}

#endif  // defined(OS_ANDROID)
