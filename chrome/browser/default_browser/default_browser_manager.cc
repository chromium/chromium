// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_manager.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_monitor.h"
#include "chrome/browser/default_browser/default_browser_notification_observer.h"
#include "chrome/browser/default_browser/setters/shell_integration_default_browser_setter.h"
#include "chrome/browser/shell_integration.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#endif  // BUILDFLAG(IS_WIN)

namespace default_browser {

namespace {

class ShellDelegateImpl : public DefaultBrowserManager::ShellDelegate {
 public:
  ShellDelegateImpl() = default;
  ~ShellDelegateImpl() override = default;
  ShellDelegateImpl(const ShellDelegateImpl&) = delete;
  ShellDelegateImpl& operator=(const ShellDelegateImpl&) = delete;

  void StartCheckIsDefault(
      shell_integration::DefaultWebClientWorkerCallback callback) override {
    auto worker =
        base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();
    worker->StartCheckIsDefault(std::move(callback));
  }

#if BUILDFLAG(IS_WIN)
  void StartCheckDefaultClientProgId(
      const GURL& scheme,
      base::OnceCallback<void(const std::u16string&)> callback) override {
    auto worker =
        base::MakeRefCounted<shell_integration::DefaultSchemeClientWorker>(
            scheme);
    worker->StartCheckIsDefaultAndGetDefaultClientProgId(base::BindOnce(
        [](base::OnceCallback<void(const std::u16string&)>
               prog_id_handle_callback,
           shell_integration::DefaultWebClientState,
           const std::u16string& prog_id) {
          std::move(prog_id_handle_callback).Run(prog_id);
        },
        std::move(callback)));
  }
#endif  // BUILDFLAG(IS_WIN)
};

// UMA enum for logging browser state validation result.
//
// LINT.IfChange(DefaultBrowserStateValidationResult)
enum class DefaultBrowserStateValidationResult {
  kTruePositive = 0,
  kTrueNegative = 1,
  kFalsePositive = 2,
  kFalseNegative = 3,
  kMaxValue = kFalseNegative
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:DefaultBrowserStateValidationResult)

#if BUILDFLAG(IS_WIN)
constexpr wchar_t kHttpUserChoiceKeyPath[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\htt"
    L"p\\UserChoice";

std::wstring GetProgIdFromRegistry(const wchar_t user_choice_path[]) {
  const wchar_t kProgIdEntryName[] = L"ProgId";

  base::win::RegKey key;
  std::wstring prog_id;

  // First, check the current user's choice.
  if (key.Open(HKEY_CURRENT_USER, user_choice_path, KEY_READ) ==
      ERROR_SUCCESS) {
    if (key.ReadValue(kProgIdEntryName, &prog_id) == ERROR_SUCCESS) {
      return prog_id;
    }
  }
  return L"";
}

// Returns whether a give program ID belongs to Chrome.
constexpr bool IsProgIdChrome(const std::u16string& prog_id) {
  constexpr std::array<std::u16string_view, 5> kChromeProgIds = {
      u"ChromeHTML", u"ChromeBHTML", u"ChromeDHTML", u"ChromeSSHTM",
      u"ChromiumHTM"};

  return std::find(kChromeProgIds.begin(), kChromeProgIds.end(), prog_id) !=
         kChromeProgIds.end();
}

// Reports whether the default browser state matches the current default program
// ID for HTTP.
void CompareHttpProgIdWithDefaultState(DefaultBrowserState default_state,
                                       const std::string_view histogram_name,
                                       const std::u16string& http_prog_id) {
  CHECK(default_state == shell_integration::IS_DEFAULT ||
        default_state == shell_integration::NOT_DEFAULT);
  const bool is_http_prog_id_chrome = IsProgIdChrome(http_prog_id);
  const bool is_default = default_state == shell_integration::IS_DEFAULT;

  DefaultBrowserStateValidationResult result;
  if (is_default && is_http_prog_id_chrome) {
    result = DefaultBrowserStateValidationResult::kTruePositive;
  } else if (!is_default && !is_http_prog_id_chrome) {
    result = DefaultBrowserStateValidationResult::kTrueNegative;
  } else if (is_default && !is_http_prog_id_chrome) {
    result = DefaultBrowserStateValidationResult::kFalsePositive;
  } else {
    result = DefaultBrowserStateValidationResult::kFalseNegative;
  }

  base::UmaHistogramEnumeration(histogram_name, result);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

DefaultBrowserManager::ShellDelegate::~ShellDelegate() = default;

DEFINE_USER_DATA(DefaultBrowserManager);

DefaultBrowserManager::DefaultBrowserManager(
    BrowserProcess* browser_process,
    std::unique_ptr<ShellDelegate> shell_delegate,
    ProfileProviderCallback profile_provider_callback)
    : shell_delegate_(std::move(shell_delegate)),
      profile_provider_callback_(std::move(profile_provider_callback)),
      scoped_unowned_user_data_(browser_process->GetUnownedUserDataHost(),
                                *this) {
  CHECK(!profile_provider_callback_.is_null());
  if (IsDefaultBrowserFrameworkEnabled()) {
    monitor_ = std::make_unique<DefaultBrowserMonitor>();

    monitor_subscription_ = monitor_->RegisterDefaultBrowserChanged(
        base::BindRepeating(&DefaultBrowserManager::OnMonitorDetectedChange,
                            base::Unretained(this)));
    if (IsDefaultBrowserChangedOsNotificationEnabled()) {
      notification_observer_ =
          std::make_unique<DefaultBrowserNotificationObserver>(
              base::BindOnce(
                  &DefaultBrowserManager::RegisterDefaultBrowserChanged,
                  base::Unretained(this)),
              base::BindOnce(&DefaultBrowserManager::GetDefaultBrowserState,
                             base::Unretained(this)),
              *this);
    }

    monitor_->StartMonitor();
  }
}

DefaultBrowserManager::~DefaultBrowserManager() = default;

// static
DefaultBrowserManager* DefaultBrowserManager::From(
    BrowserProcess* browser_process) {
  return browser_process ? Get(browser_process->GetUnownedUserDataHost())
                         : nullptr;
}

// static
std::unique_ptr<DefaultBrowserManager::ShellDelegate>
DefaultBrowserManager::CreateDefaultDelegate() {
  return std::make_unique<ShellDelegateImpl>();
}

// static
std::unique_ptr<DefaultBrowserController>
DefaultBrowserManager::CreateControllerFor(
    DefaultBrowserEntrypointType entrypoint) {
  return std::make_unique<DefaultBrowserController>(
      std::make_unique<ShellIntegrationDefaultBrowserSetter>(), entrypoint);
}

Profile& DefaultBrowserManager::GetProfile() {
  return *profile_provider_callback_.Run();
}

void DefaultBrowserManager::GetDefaultBrowserState(
    DefaultBrowserCheckCompletionCallback callback) {
  shell_delegate_->StartCheckIsDefault(
      base::BindOnce(&DefaultBrowserManager::OnDefaultBrowserCheckResult,
                     base::Unretained(this), std::move(callback)));
}

void DefaultBrowserManager::OnDefaultBrowserCheckResult(
    DefaultBrowserCheckCompletionCallback callback,
    DefaultBrowserState default_state) {
  if (default_state == shell_integration::IS_DEFAULT ||
      default_state == shell_integration::NOT_DEFAULT) {
    if (base::FeatureList::IsEnabled(kPerformDefaultBrowserCheckValidations)) {
      PerformDefaultBrowserCheckValidations(default_state);
    }
  }
  std::move(callback).Run(default_state);
}

void DefaultBrowserManager::PerformDefaultBrowserCheckValidations(
    DefaultBrowserState default_state) {
#if BUILDFLAG(IS_WIN)
  shell_delegate_->StartCheckDefaultClientProgId(
      GURL("http://"),
      base::BindOnce(&CompareHttpProgIdWithDefaultState, default_state,
                     "DefaultBrowser.HttpProgIdAssocValidationResult"));
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](DefaultBrowserState default_state) {
            std::wstring prog_id_from_registry =
                GetProgIdFromRegistry(kHttpUserChoiceKeyPath);
            CompareHttpProgIdWithDefaultState(
                default_state,
                "DefaultBrowser.HttpProgIdRegistryValidationResult",
                base::WideToUTF16(prog_id_from_registry));
          },
          default_state));
#endif  // BUILDFLAG(IS_WIN)
}

base::CallbackListSubscription
DefaultBrowserManager::RegisterDefaultBrowserChanged(
    DefaultBrowserChangedCallback callback) {
  return observers_.Add(std::move(callback));
}

void DefaultBrowserManager::OnMonitorDetectedChange() {
  GetDefaultBrowserState(base::BindOnce(&DefaultBrowserManager::NotifyObservers,
                                        base::Unretained(this)));
}

void DefaultBrowserManager::NotifyObservers(DefaultBrowserState state) {
  observers_.Notify(state);
}

}  // namespace default_browser
