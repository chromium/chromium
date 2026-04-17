// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/entra_provider_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/android/chrome_jni_headers/PlatformAuthEntraTokensReader_jni.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/common/policy_logger.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

namespace enterprise_auth {

namespace {

using OnJavaReadTokensCallback = EntraProviderAndroid::OnJavaReadTokensCallback;
using Status = EntraProviderAndroid::Status;
using AuthenticationResult = EntraProviderAndroid::AuthenticationResult;

static constexpr auto kSupportedOrigins =
    base::MakeFixedFlatSet<std::string_view>(
        {"https://login.microsoftonline.com", "https://login.microsoft.com",
         "https://login.windows.net", "https://login.microsoftonline.us",
         "https://login.partner.microsoftonline.cn"});

static constexpr char kHeaderNamePrefix[] = "x-ms-";

static constexpr char kLogTag[] = "[Android Entra SSO]";

std::string StatusToString(Status status) {
  switch (status) {
    case Status::kUnexpectedError:
      return "kUnexpectedError";
    case Status::kSignatureVerificationFailed:
      return "kSignatureVerificationFailed";
    case Status::kInvalidBundleFormat:
      return "kInvalidBundleFormat";
    case Status::kNoBundleResult:
      return "kNoBundleResult";
    case Status::kBundleResultContainsEntraError:
      return "kBundleResultContainsEntraError";
    case Status::kBundleResultContainsOsError:
      return "kBundleResultContainsOsError";
    case Status::kUnexpectedPackageProvider:
      return "kUnexpectedPackageProvider";
    case Status::kDisallowedDebugPackageProvider:
      return "kDisallowedDebugPackageProvider";
    case Status::kTimeout:
      return "kTimeout";
    case Status::kJsonParsingFailed:
      return "kJsonParsingFailed";
    case Status::kAllHeadersSkipped:
      return "kAllHeadersSkipped";
    case Status::kOk:
      return "kOk";
    case Status::kNoBrokerRegistered:
      return "kNoBrokerRegistered";
  }
}

void RecordResultMetrics(AuthenticationResult result,
                         base::TimeTicks start_time) {
  base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  base::UmaHistogramEnumeration(
      EntraProviderAndroid::kAuthenticationResultHistogram, result);
  switch (result) {
    case AuthenticationResult::kSuccessWithHeaders:
    case AuthenticationResult::kSuccessWithNoHeaders:
      base::UmaHistogramTimes(EntraProviderAndroid::kDurationSuccessHistogram,
                              std::move(duration));
      break;
    case AuthenticationResult::kNoBrokerRegistered:
      base::UmaHistogramTimes(EntraProviderAndroid::kDurationNoBrokerHistogram,
                              std::move(duration));
      break;
    case AuthenticationResult::kFailure:
      base::UmaHistogramTimes(EntraProviderAndroid::kDurationFailureHistogram,
                              std::move(duration));
      break;
  }
}

void RecordFailureMetrics(Status status, base::TimeTicks start_time) {
  DCHECK_LE(status, Status::kMaxFailureReason);
  base::UmaHistogramEnumeration(EntraProviderAndroid::kFailureReasonHistogram,
                                status);
  RecordResultMetrics(AuthenticationResult::kFailure, std::move(start_time));
}

void InvokeJavaReadTokens(const std::string& url,
                          OnJavaReadTokensCallback callback) {
  auto bridge_callback = base::BindOnce(
      [](OnJavaReadTokensCallback final_callback, int status_int,
         std::string result) {
        std::move(final_callback)
            .Run(static_cast<Status>(status_int), std::move(result));
      },
      std::move(callback));

  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_PlatformAuthEntraTokensReader_readTokens(env, url,
                                                std::move(bridge_callback));
}

}  // namespace

EntraProviderAndroid::EntraProviderAndroid() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAndroidEntraSsoAllowDebugBrokers)) {
    LOG_POLICY(WARNING, POLICY_AUTH)
        << kLogTag
        << " Entra SSO will accept authentication tokens from non-production "
           "broker apps. This should be used ONLY for testing. To disable "
           "rerun chrome without commandline flag "
        << switches::kAndroidEntraSsoAllowDebugBrokers;
  }
}

EntraProviderAndroid::~EntraProviderAndroid() = default;

bool EntraProviderAndroid::SupportsOriginFiltering() {
  return true;
}

void EntraProviderAndroid::FetchOrigins(
    FetchOriginsCallback on_fetch_complete) {
  auto origins = std::make_unique<std::vector<url::Origin>>();
  for (const std::string_view origin : kSupportedOrigins) {
    origins->push_back(url::Origin::Create(GURL(origin)));
  }
  std::move(on_fetch_complete).Run(std::move(origins));
}

void EntraProviderAndroid::GetData(
    const GURL& url,
    PlatformAuthProviderManager::GetDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
  if (sso_disabled_) {
    VLOG_POLICY(2, POLICY_AUTH)
        << kLogTag << " skipping fetching headers for " << url.GetHost()
        << " because the SSO has been disabled.";
    std::move(callback).Run({});
    return;
  }
  VLOG_POLICY(2, POLICY_AUTH)
      << kLogTag << " fetching headers for " << url.spec();

  // Binds OnJavaHeadersRead to the main thread with the
  // PlatformAuthProviderManager's callback.
  auto result_callback = base::BindOnce(
      &EntraProviderAndroid::OnJavaHeadersRead, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), base::TimeTicks::Now());
  auto thread_safe_callback =
      base::BindPostTaskToCurrentDefault(std::move(result_callback));

  // Create a task to run on a thread pool which will synchronously read tokens
  // from the Android OS.
  base::OnceClosure task;
  if (mock_java_read_tokens_) {
    CHECK_IS_TEST();
    task =
        base::BindOnce(mock_java_read_tokens_, std::move(thread_safe_callback));
  } else {
    task = base::BindOnce(&InvokeJavaReadTokens, std::string(url.spec()),
                          std::move(thread_safe_callback));
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      std::move(task));
}

void EntraProviderAndroid::OnJavaHeadersRead(
    PlatformAuthProviderManager::GetDataCallback callback,
    base::TimeTicks start_time,
    Status status,
    std::string result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
  switch (status) {
    case Status::kOk:
      ParseJsonHeaders(std::move(callback), result, std::move(start_time));
      return;
    case Status::kNoBrokerRegistered:
      LOG_POLICY(WARNING, POLICY_AUTH)
          << kLogTag << " tokens fetching failed with "
          << StatusToString(status) << ": " << result;
      sso_disabled_ = true;
      RecordResultMetrics(AuthenticationResult::kNoBrokerRegistered,
                          std::move(start_time));
      std::move(callback).Run({});
      return;
    default:
      LOG_POLICY(ERROR, POLICY_AUTH)
          << kLogTag << " tokens fetching failed with "
          << StatusToString(status) << ": " << result;
      sso_disabled_ = true;
      RecordFailureMetrics(status, std::move(start_time));
      std::move(callback).Run({});
      return;
  }
}

void EntraProviderAndroid::ParseJsonHeaders(
    PlatformAuthProviderManager::GetDataCallback callback,
    std::string_view headers_raw_json,
    base::TimeTicks start_time) {
  const std::optional<base::DictValue> headers_dict =
      base::JSONReader::ReadDict(
          headers_raw_json,
          base::JSON_PARSE_RFC | base::JSON_ALLOW_TRAILING_COMMAS);
  if (!headers_dict.has_value()) {
    LOG_POLICY(ERROR, POLICY_AUTH)
        << kLogTag
        << " Getting authentication tokens failed! The JSON "
           "could not be parsed.";
    RecordFailureMetrics(Status::kJsonParsingFailed, std::move(start_time));
    sso_disabled_ = true;
    std::move(callback).Run({});
    return;
  }

  const base::DictValue* const headers_field =
      headers_dict.value().FindDict("headers");
  if (!headers_field) {
    LOG_POLICY(ERROR, POLICY_AUTH)
        << kLogTag
        << " Getting authentication tokens failed! The JSON "
           "returned by the AccountManager is missing `headers` dict entry.";
    RecordFailureMetrics(Status::kJsonParsingFailed, std::move(start_time));
    sso_disabled_ = true;
    std::move(callback).Run({});
    return;
  }

  net::HttpRequestHeaders result_headers;
  for (const auto [key, value] : *headers_field) {
    if (!base::StartsWith(key, kHeaderNamePrefix,
                          base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    if (!net::HttpUtil::IsValidHeaderName(key)) {
      LOG_POLICY(WARNING, POLICY_AUTH)
          << kLogTag << " Skipping invalid header name: " << key;
      continue;
    }

    const std::string* const str_value = value.GetIfString();
    if (!str_value) {
      LOG_POLICY(WARNING, POLICY_AUTH)
          << kLogTag << " Invalid header value type for key " << key << ": "
          << value.DebugString();
      continue;
    }

    if (net::HttpUtil::IsValidHeaderValue(*str_value)) {
      result_headers.SetHeader(key, *str_value);
    } else {
      LOG_POLICY(WARNING, POLICY_AUTH)
          << kLogTag << " Invalid header name: value pair " << key << ": "
          << *str_value;
    }
  }

  if (result_headers.IsEmpty() && !headers_field->empty()) {
    LOG_POLICY(ERROR, POLICY_AUTH) << kLogTag << " all headers were skipped.";
    RecordFailureMetrics(Status::kAllHeadersSkipped, std::move(start_time));
    std::move(callback).Run({});
    return;
  }

  VLOG_POLICY(2, POLICY_AUTH)
      << kLogTag << " Attaching this number of headers to the request: "
      << result_headers.GetHeaderVector().size();
  if (result_headers.IsEmpty()) {
    RecordResultMetrics(AuthenticationResult::kSuccessWithNoHeaders,
                        std::move(start_time));
  } else {
    RecordResultMetrics(AuthenticationResult::kSuccessWithHeaders,
                        std::move(start_time));
  }
  std::move(callback).Run(std::move(result_headers));
}

}  // namespace enterprise_auth
