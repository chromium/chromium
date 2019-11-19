// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/rlz/rlz_ping_handler.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/android/chrome_jni_headers/RlzPingHandler_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/net_response_check.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using base::android::JavaRef;

constexpr int kMaxRetries = 10;

namespace {
const char kProtocolCgiVariable[] = "rep";
}

namespace chrome {
namespace android {

RlzPingHandler::RlzPingHandler(const JavaRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);
  url_loader_factory_ =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
}

RlzPingHandler::~RlzPingHandler() = default;

void RlzPingHandler::Ping(
    const base::android::JavaParamRef<jstring>& j_brand,
    const base::android::JavaParamRef<jstring>& j_language,
    const base::android::JavaParamRef<jstring>& j_events,
    const base::android::JavaParamRef<jstring>& j_id,
    const base::android::JavaParamRef<jobject>& j_callback) {
  if (!j_brand || !j_language || !j_events || !j_id || !j_callback) {
    base::android::RunBooleanCallbackAndroid(j_callback, false);
    delete this;
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  j_callback_.Reset(env, j_callback);
  std::string brand = ConvertJavaStringToUTF8(env, j_brand);
  std::string language = ConvertJavaStringToUTF8(env, j_language);
  std::string events = ConvertJavaStringToUTF8(env, j_events);
  std::string id = ConvertJavaStringToUTF8(env, j_id);

  DCHECK_EQ(brand.length(), 4u);
  DCHECK_EQ(language.length(), 2u);
  DCHECK_EQ(id.length(), 50u);

  GURL request_url(base::StringPrintf(
      "https://%s%s?", rlz_lib::kFinancialServer, rlz_lib::kFinancialPingPath));
  request_url = net::AppendQueryParameter(
      request_url, rlz_lib::kProductSignatureCgiVariable, "chrome");
  request_url =
      net::AppendQueryParameter(request_url, kProtocolCgiVariable, "1");
  request_url = net::AppendQueryParameter(
      request_url, rlz_lib::kProductBrandCgiVariable, brand);
  request_url = net::AppendQueryParameter(
      request_url, rlz_lib::kProductLanguageCgiVariable, language);
  request_url = net::AppendQueryParameter(request_url,
                                          rlz_lib::kEventsCgiVariable, events);
  request_url = net::AppendQueryParameter(request_url,
                                          rlz_lib::kMachineIdCgiVariable, id);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("rlz", R"(
          semantics {
            sender: "RLZ Ping Handler"
            description:
            "Sends rlz pings for revenue related tracking to the designated web"
            "end point."
            trigger:
            "Critical signals like first install, a promotion dialog being"
            "shown, a user selection for a promotion may trigger a ping"
            destination: WEBSITE
          }
          policy {
            cookies_allowed: NO
            cookies_store: "user"
            setting: "Not user controlled. But it uses a trusted web end point"
                     "that doesn't use user data"
            policy_exception_justification:
              "Not implemented, considered not useful as no content is being "
              "uploaded."
          })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_5XX |
                       network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&RlzPingHandler::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void RlzPingHandler::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  bool valid = false;
  if (!response_body) {
    int response_code = -1;
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code =
          simple_url_loader_->ResponseInfo()->headers->response_code();
    }
    LOG(WARNING) << base::StringPrintf("Rlz endpoint responded with code %d.",
                                       response_code);
  } else {
    int response_length = -1;
    valid =
        rlz_lib::IsPingResponseValid(response_body->c_str(), &response_length);
  }

  // TODO(yusufo) : Investigate what else can be checked for validity that is
  // specific to the ping
  base::android::RunBooleanCallbackAndroid(j_callback_, valid);
  delete this;
}

void JNI_RlzPingHandler_StartPing(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jstring>& j_brand,
    const base::android::JavaParamRef<jstring>& j_language,
    const base::android::JavaParamRef<jstring>& j_events,
    const base::android::JavaParamRef<jstring>& j_id,
    const base::android::JavaParamRef<jobject>& j_callback) {
  RlzPingHandler* handler = new RlzPingHandler(j_profile);
  handler->Ping(j_brand, j_language, j_events, j_id, j_callback);
}

}  // namespace android
}  // namespace chrome
