// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/dns_probe_runner.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/pref_names.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/privacy/jni_headers/SecureDnsBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using chrome_browser_net::DnsProbeRunner;

namespace secure_dns = chrome_browser_net::secure_dns;

namespace {

net::DohProviderEntry::List GetFilteredProviders() {
  // Note: Check whether each provider is enabled *after* filtering based on
  // country code so that if we are doing experimentation via Finch for a
  // regional provider, the experiment groups will be less likely to include
  // users from other regions unnecessarily (since a client will be included in
  // the experiment if the provider feature flag is checked).
  return secure_dns::SelectEnabledProviders(secure_dns::ProvidersForCountry(
      net::DohProviderEntry::GetList(), country_codes::GetCurrentCountryID()));
}

// Runs a DNS probe according to the configuration in |overrides|,
// asynchronously sets |success| to indicate the result, and signals
// |waiter| when the probe has completed.  Must run on the UI thread.
void RunProbe(base::WaitableEvent* waiter,
              bool* success,
              const std::string& doh_config) {
  std::optional<net::DnsOverHttpsConfig> parsed =
      net::DnsOverHttpsConfig::FromString(doh_config);
  DCHECK(parsed.has_value());  // `doh_config` must be valid.
  auto* manager = g_browser_process->system_network_context_manager();
  auto runner = secure_dns::MakeProbeRunner(
      std::move(*parsed),
      base::BindRepeating(&SystemNetworkContextManager::GetContext,
                          base::Unretained(manager)));
  auto* const runner_ptr = runner.get();
  runner_ptr->RunProbe(base::BindOnce(
      [](base::WaitableEvent* waiter, bool* success,
         std::unique_ptr<DnsProbeRunner> runner) {
        *success = runner->result() == DnsProbeRunner::CORRECT;
        waiter->Signal();
      },
      waiter, success, std::move(runner)));
}

}  // namespace

static jint JNI_SecureDnsBridge_GetMode(JNIEnv* env) {
  return static_cast<int>(
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              true /* force_check_parental_controls_for_automatic_mode */)
          .mode());
}

static void JNI_SecureDnsBridge_SetMode(JNIEnv* env, jint mode) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(
      prefs::kDnsOverHttpsMode,
      SecureDnsConfig::ModeToString(static_cast<net::SecureDnsMode>(mode)));
}

static jboolean JNI_SecureDnsBridge_IsModeManaged(JNIEnv* env) {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->IsManagedPreference(prefs::kDnsOverHttpsMode);
}

static ScopedJavaLocalRef<jobjectArray> JNI_SecureDnsBridge_GetProviders(
    JNIEnv* env) {
  net::DohProviderEntry::List providers = GetFilteredProviders();
  std::vector<std::vector<std::u16string>> ret;
  ret.reserve(providers.size());
  base::ranges::transform(
      providers, std::back_inserter(ret),
      [](const net::DohProviderEntry* entry) -> std::vector<std::u16string> {
        net::DnsOverHttpsConfig config({entry->doh_server_config});
        return {base::UTF8ToUTF16(entry->ui_name),
                base::UTF8ToUTF16(config.ToString()),
                base::UTF8ToUTF16(entry->privacy_policy)};
      });
  return base::android::ToJavaArrayOfStringArray(env, ret);
}

static ScopedJavaLocalRef<jstring> JNI_SecureDnsBridge_GetConfig(JNIEnv* env) {
  PrefService* local_state = g_browser_process->local_state();
  return base::android::ConvertUTF8ToJavaString(
      env, local_state->GetString(prefs::kDnsOverHttpsTemplates));
}

static jboolean JNI_SecureDnsBridge_SetConfig(
    JNIEnv* env,
    const JavaParamRef<jstring>& jconfig) {
  PrefService* local_state = g_browser_process->local_state();
  std::string config = base::android::ConvertJavaStringToUTF8(jconfig);
  if (config.empty()) {
    local_state->ClearPref(prefs::kDnsOverHttpsTemplates);
    return true;
  }

  if (net::DnsOverHttpsConfig::FromString(config).has_value()) {
    local_state->SetString(prefs::kDnsOverHttpsTemplates, config);
    return true;
  }

  return false;
}

static jint JNI_SecureDnsBridge_GetManagementMode(JNIEnv* env) {
  return static_cast<int>(
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetSecureDnsConfiguration(
              true /* force_check_parental_controls_for_automatic_mode */)
          .management_mode());
}

static void JNI_SecureDnsBridge_UpdateValidationHistogram(JNIEnv* env,
                                                          jboolean valid) {
  secure_dns::UpdateValidationHistogram(valid);
}

static jboolean JNI_SecureDnsBridge_ProbeConfig(
    JNIEnv* env,
    const JavaParamRef<jstring>& doh_config) {
  // Android recommends converting async functions to blocking when using JNI:
  // https://developer.android.com/training/articles/perf-jni.
  // This function converts the DnsProbeRunner, which can only be created and
  // used on the UI thread, into a blocking function that can be called from an
  // auxiliary Java thread.
  // TODO: Use std::future if allowed in the future.
  base::WaitableEvent waiter;
  bool success;
  bool posted = content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(RunProbe, &waiter, &success,
                     base::android::ConvertJavaStringToUTF8(doh_config)));
  DCHECK(posted);
  waiter.Wait();

  secure_dns::UpdateProbeHistogram(success);
  return success;
}
