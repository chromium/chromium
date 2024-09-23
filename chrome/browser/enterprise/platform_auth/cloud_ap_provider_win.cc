// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/platform_auth/cloud_ap_provider_win.h"

#include <objbase.h>

#include <windows.h>

#include <lmcons.h>
#include <lmjoin.h>
#include <proofofpossessioncookieinfo.h>
#include <stdint.h>
#include <windows.security.authentication.web.core.h>
#include <wrl/client.h>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/timer/elapsed_timer.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/scoped_hstring.h"
#include "chrome/browser/enterprise/platform_auth/cloud_ap_utils_win.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Security::Authentication::Web::Core::
    IWebAuthenticationCoreManagerStatics;
using ABI::Windows::Security::Credentials::IWebAccountProvider;
using ABI::Windows::Security::Credentials::WebAccountProvider;
using Microsoft::WRL::ComPtr;

namespace enterprise_auth {

namespace {

using OnSupportLevelCallback =
    base::OnceCallback<void(CloudApProviderWin::SupportLevel)>;

// A helper to manage the lifetime of various objects while checking to see if
// there is at least one WebAccount for the default provider.
class WebAccountSupportFinder
    : public base::RefCountedThreadSafe<WebAccountSupportFinder> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  // `on_support_level` is posted to `result_runner` upon destruction with the
  // results of the operation. Reports `SupportLevel::kEnabled` if an account is
  // found, `SupportLevel::kDisabled` if no account is found, or
  // `SupportLevel::kUnsupported` in case of any error.
  WebAccountSupportFinder(scoped_refptr<base::TaskRunner> result_runner,
                          OnSupportLevelCallback on_support_level)
      : result_runner_(std::move(result_runner)),
        on_support_level_(std::move(on_support_level)) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  WebAccountSupportFinder(const WebAccountSupportFinder&) = delete;
  WebAccountSupportFinder& operator=(const WebAccountSupportFinder&) = delete;

  // Starts the operation.
  void Find() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::win::AssertComApartmentType(base::win::ComApartmentType::MTA);

    // Get the `WebAuthenticationCoreManager`.
    ComPtr<IWebAuthenticationCoreManagerStatics> auth_manager;
    HRESULT hresult = base::win::GetActivationFactory<
        IWebAuthenticationCoreManagerStatics,
        RuntimeClass_Windows_Security_Authentication_Web_Core_WebAuthenticationCoreManager>(
        &auth_manager);
    if (FAILED(hresult))
      return;  // Unsupported.

    ComPtr<IAsyncOperation<WebAccountProvider*>> find_provider_op;

    // "https://login.windows.local" -- account provider for the OS. Don't
    // specify an authority when using it.
    // https://docs.microsoft.com/en-us/uwp/api/windows.security.authentication.web.core.webauthenticationcoremanager.findaccountproviderasync?view=winrt-19041
    hresult = auth_manager->FindAccountProviderAsync(
        base::win::ScopedHString::Create(L"https://login.windows.local").get(),
        &find_provider_op);
    if (FAILED(hresult))
      return;  // Unsupported.

    hresult = base::win::PostAsyncHandlers(
        find_provider_op.Get(),
        base::BindOnce(&WebAccountSupportFinder::OnAccountProvider,
                       base::WrapRefCounted(this)));
    if (FAILED(hresult)) {
      DLOG(ERROR)
          << __func__
          << ": Failed to post result task for provider fetch; HRESULT = "
          << std::hex << hresult;
    }
  }

 private:
  friend class base::RefCountedThreadSafe<WebAccountSupportFinder>;

  // Posts `on_support_level_` with `support_level_` to `result_runner_`.
  ~WebAccountSupportFinder() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    result_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_support_level_), support_level_));
  }

  // Handles the result of a successful call to `FindAccountProviderAsync()`.
  void OnAccountProvider(ComPtr<IWebAccountProvider> account_provider) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Regardless of whether a provider is found, the machine supports account
    // providers.
    support_level_ = account_provider
                         ? CloudApProviderWin::SupportLevel::kEnabled
                         : CloudApProviderWin::SupportLevel::kDisabled;
  }

  scoped_refptr<base::TaskRunner> result_runner_;
  OnSupportLevelCallback on_support_level_;
  CloudApProviderWin::SupportLevel support_level_ =
      CloudApProviderWin::SupportLevel::kUnsupported;
  SEQUENCE_CHECKER(sequence_checker_);
};

CloudApProviderWin::SupportLevel* support_level_for_testing_ = nullptr;

// Returns the platform's ProofOfPossessionCookieInfoManager, or null if
// unsupported. `hresult_out`, if provided, is populated with the result of
// object creation.
ComPtr<IProofOfPossessionCookieInfoManager> MakeCookieInfoManager(
    HRESULT* hresult_out = nullptr) {
  // CLSID_ProofOfPossessionCookieInfoManager from
  // ProofOfPossessionCookieInfo.h.
  static constexpr CLSID kClsidProofOfPossessionCookieInfoManager = {
      0xA9927F85,
      0xA304,
      0x4390,
      {0x8B, 0x23, 0xA7, 0x5F, 0x1C, 0x66, 0x86, 0x00}};

  // There is no need for SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY here
  // since this task is posted at USER_VISIBLE priority.
  DCHECK_NE(base::PlatformThread::GetCurrentThreadType(),
            base::ThreadType::kBackground);
  base::win::AssertComInitialized();

  ComPtr<IProofOfPossessionCookieInfoManager> manager;

  HRESULT hresult = ::CoCreateInstance(
      kClsidProofOfPossessionCookieInfoManager,
      /*pUnkOuter=*/nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager));
  if (hresult_out)
    *hresult_out = hresult;
  return SUCCEEDED(hresult) ? manager : nullptr;
}

void ParseCookieInfo(const ProofOfPossessionCookieInfo* cookie_info,
                     const DWORD cookie_info_count,
                     net::HttpRequestHeaders& auth_headers) {
  net::cookie_util::ParsedRequestCookies parsed_cookies;

  // If the auth cookie name begins with 'x-ms-', attach the cookie as a
  // new header. Otherwise, append it to the existing list of cookies.
  static constexpr std::string_view kHeaderPrefix("x-ms-");
  for (DWORD i = 0; i < cookie_info_count; ++i) {
    const ProofOfPossessionCookieInfo& cookie = cookie_info[i];
    auto ascii_cookie_name = base::WideToASCII(cookie.name);
    if (base::StartsWith(ascii_cookie_name, kHeaderPrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      // Removing cookie attributes from the value before setting it as a
      // header.
      std::string ascii_cookie_value = base::WideToASCII(cookie.data);
      std::string::size_type cookie_attributes_position =
          ascii_cookie_value.find(";");
      if (cookie_attributes_position != std::string::npos) {
        ascii_cookie_value =
            ascii_cookie_value.substr(0, cookie_attributes_position);
      }
      auth_headers.SetHeader(std::move(ascii_cookie_name),
                             std::move(ascii_cookie_value));
    } else {
      parsed_cookies.emplace_back(std::move(ascii_cookie_name),
                                  base::WideToASCII(cookie.data));
    }
  }

  if (parsed_cookies.size() > 0) {
    auth_headers.SetHeader(
        net::HttpRequestHeaders::kCookie,
        net::cookie_util::SerializeRequestCookieLine(parsed_cookies));
  }
}

// Returns the proof-of-possession cookies and headers for the interactive
// user to authenticate to the IdP/STS at `url`.
net::HttpRequestHeaders GetAuthData(const GURL& url) {
  base::win::AssertComInitialized();
  DCHECK(url.is_valid());

  net::HttpRequestHeaders auth_headers;
  DWORD cookie_info_count = 0;
  base::ElapsedTimer elapsed_timer;

  HRESULT hresult = S_OK;
  auto manager = MakeCookieInfoManager(&hresult);
  if (manager) {
    ProofOfPossessionCookieInfo* cookie_info = nullptr;
    hresult =
        manager->GetCookieInfoForUri(base::ASCIIToWide(url.spec()).c_str(),
                                     &cookie_info_count, &cookie_info);
    if (SUCCEEDED(hresult)) {
      DCHECK(!cookie_info_count || cookie_info);
      ParseCookieInfo(cookie_info, cookie_info_count, auth_headers);
      if (cookie_info)
        FreeProofOfPossessionCookieInfoArray(cookie_info, cookie_info_count);
    }
  }
  const auto delta = elapsed_timer.Elapsed();

  if (SUCCEEDED(hresult)) {
    base::UmaHistogramTimes("Enterprise.PlatformAuth.GetAuthData.SuccessTime",
                            delta);
    base::UmaHistogramExactLinear("Enterprise.PlatformAuth.GetAuthData.Count",
                                  cookie_info_count,
                                  10);  // Expect < 10 cookies.
  } else {
    base::UmaHistogramTimes("Enterprise.PlatformAuth.GetAuthData.FailureTime",
                            delta);
    base::UmaHistogramSparse(
        "Enterprise.PlatformAuth.GetAuthData.FailureHresult", int{hresult});
  }

  return auth_headers;
}

// Returns the support level based on Azure AD join status.
CloudApProviderWin::SupportLevel GetAadJoinSupportLevel() {
  // There is no need for `SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY` here
  // since this task is posted at `USER_VISIBLE` priority.
  DCHECK_NE(base::PlatformThread::GetCurrentThreadType(),
            base::ThreadType::kBackground);

  // If Azure AD join info retrieval fails, this feature is not supported.
  PDSREG_JOIN_INFO join_info = nullptr;
  if (FAILED(::NetGetAadJoinInformation(/*pcszTenantId=*/nullptr, &join_info)))
    return CloudApProviderWin::SupportLevel::kUnsupported;

  // Azure AD join info was retrieved successfully, so the feature is supported.
  // This will free the retrieved Azure AD join info after going out of scope.
  std::unique_ptr<DSREG_JOIN_INFO, decltype(&NetFreeAadJoinInformation)>
      scoped_join_info(join_info, ::NetFreeAadJoinInformation);

  return (!join_info || join_info->joinType == DSREG_UNKNOWN_JOIN)
             ? CloudApProviderWin::SupportLevel::kDisabled
             : CloudApProviderWin::SupportLevel::kEnabled;
}

// Handles the results of checking for a WebAccount from the default provider.
void OnFindWebAccount(OnSupportLevelCallback on_support_level,
                      CloudApProviderWin::SupportLevel support_level) {
  // Full support if there's at least one WebAccount for the default provider.
  if (support_level == CloudApProviderWin::SupportLevel::kEnabled) {
    std::move(on_support_level).Run(CloudApProviderWin::SupportLevel::kEnabled);
    return;
  }

  // Otherwise, support is based on whether or not the device is AAD-joined one
  // way (device joined) or another (workplace joined).
  std::move(on_support_level).Run(GetAadJoinSupportLevel());
}

// Evaluates the level of support for Cloud AP SSO, running `on_support_level`
// on the caller's sequence (synchronously or asynchronously) with the result.
void GetSupportLevel(OnSupportLevelCallback on_support_level) {
  if (support_level_for_testing_) {
    std::move(on_support_level).Run(*support_level_for_testing_);
    return;
  }

  // Check if the machine has the ProofOfPossessionCookieInfoManager COM class.
  if (!MakeCookieInfoManager()) {
    std::move(on_support_level)
        .Run(CloudApProviderWin::SupportLevel::kUnsupported);
    return;
  }

  // Check if there's at least one WebAccount for the default provider.
  base::MakeRefCounted<WebAccountSupportFinder>(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&OnFindWebAccount, std::move(on_support_level)))
      ->Find();
}

// Reads the IdP origins from the Windows registry.
std::vector<url::Origin> ReadOrigins() {
  static constexpr wchar_t kLoginUri[] = L"LoginUri";
  std::vector<url::Origin> result;

  // Windows registry locations (provided by Microsoft) which are expected to
  // contain Microsoft IdP origins.
  AppendRegistryOrigins(HKEY_LOCAL_MACHINE,
                        L"SOFTWARE\\Microsoft\\IdentityStore\\LoadParameters\\"
                        L"{B16898C6-A148-4967-9171-64D755DA8520}",
                        kLoginUri, result);
  AppendRegistryOrigins(
      HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\IdentityStore\\Providers\\"
      L"{B16898C6-A148-4967-9171-64D755DA8520}\\LoadParameters",
      kLoginUri, result);
  AppendRegistryOrigins(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\AAD\\Package", kLoginUri,
      result);
  AppendRegistryOrigins(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\IdentityCRL",
                        L"LoginUrl", result);

  if (result.empty()) {
    // Certain legacy versions of Windows may not have origins in the registry.
    // Use the two well-known origins if none other are found.
    result.push_back(url::Origin::Create(GURL("https://login.live.com")));
    result.push_back(
        url::Origin::Create(GURL("https://login.microsoftonline.com")));
  }

  return result;
}

// Handles the results of a call to `GetSupportLevel()`.
void OnSupportLevel(scoped_refptr<base::TaskRunner> result_runner,
                    CloudApProviderWin::FetchOriginsCallback on_origins,
                    CloudApProviderWin::SupportLevel support_level) {
  std::unique_ptr<std::vector<url::Origin>> results;

  switch (support_level) {
    case CloudApProviderWin::SupportLevel::kUnsupported:
      // There is no hope in trying again.
      break;
    case CloudApProviderWin::SupportLevel::kDisabled:
      // Not joined at the moment, but could change in the future.
      results = std::make_unique<std::vector<url::Origin>>();
      break;
    case CloudApProviderWin::SupportLevel::kEnabled:
      results = std::make_unique<std::vector<url::Origin>>(ReadOrigins());
      break;
  }

  result_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_origins), std::move(results)));
}

// Fetches the collection of IdP/STS origins in the ThreadPool. Runs
// `on_origins` on `result_runner` with the origins or nullptr if Cloud AP SSO
// is not supported.
void FetchOriginsInPool(scoped_refptr<base::TaskRunner> result_runner,
                        CloudApProviderWin::FetchOriginsCallback on_origins) {
  GetSupportLevel(base::BindOnce(&OnSupportLevel, std::move(result_runner),
                                 std::move(on_origins)));
}

}  // namespace

CloudApProviderWin::CloudApProviderWin() = default;

CloudApProviderWin::~CloudApProviderWin() = default;

bool CloudApProviderWin::SupportsOriginFiltering() {
  return true;
}

void CloudApProviderWin::FetchOrigins(FetchOriginsCallback on_fetch_complete) {
  // The strategy is as follows:
  // 1. See if the ProofOfPossessionCookieInfoManager can be instantiated. If
  //    not, the platform doesn't support AAD SSO.
  // 2. See if the user has a WebAccount from the default provider. If they do,
  //    the platform supports AAD SSO and it is enabled.
  // 3. See if either the device is joined to an AAD domain or if an AAD work
  //    account has been added. In either case, the device supports AAD SSO and
  //    it is enabled.
  // 4. If checking the join status failed, the platform doesn't support AAD
  //    SSO; otherwise, the platform supports AAD SSO but it is disabled.
  // The callback is run with:
  // - nullptr if AAD SSO is not supported.
  // - an empty collection of origins if AAD SSO is supported but disabled.
  // - two or more URLs if AAD SSO is supported and enabled.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&FetchOriginsInPool,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                std::move(on_fetch_complete)));
}

void CloudApProviderWin::GetData(
    const GURL& url,
    PlatformAuthProviderManager::GetDataCallback callback) {
  get_data_subscription_ = on_get_data_callback_list_.Add(std::move(callback));
  if (!base::ThreadPool::CreateCOMSTATaskRunner(
           {base::TaskPriority::USER_BLOCKING,
            base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()})
           ->PostTaskAndReplyWithResult(
               FROM_HERE, base::BindOnce(&GetAuthData, url),
               base::BindOnce(&CloudApProviderWin::OnGetDataCallback,
                              base::Unretained(this)))) {
    OnGetDataCallback(net::HttpRequestHeaders());
  }
}

void CloudApProviderWin::OnGetDataCallback(
    net::HttpRequestHeaders auth_headers) {
  on_get_data_callback_list_.Notify(std::move(auth_headers));
}

// static
void CloudApProviderWin::SetSupportLevelForTesting(
    std::optional<SupportLevel> level) {
  delete std::exchange(support_level_for_testing_, nullptr);
  if (!level)
    return;
  support_level_for_testing_ = new SupportLevel;
  *support_level_for_testing_ = level.value();
}

void CloudApProviderWin::ParseCookieInfoForTesting(
    const ProofOfPossessionCookieInfo* cookie_info,
    const DWORD cookie_info_count,
    net::HttpRequestHeaders& auth_headers) {
  ParseCookieInfo(cookie_info, cookie_info_count, auth_headers);
}

}  // namespace enterprise_auth
