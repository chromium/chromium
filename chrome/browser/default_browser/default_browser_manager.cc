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
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_monitor.h"
#include "chrome/browser/default_browser/setters/shell_integration_default_browser_setter.h"
#include "chrome/browser/shell_integration.h"

namespace {

class ShellDelegateImpl
    : public default_browser::DefaultBrowserManager::ShellDelegate {
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
      const std::string& scheme,
      base::OnceCallback<void(const std::u16string&)> callback) override {
    // TODO(crbug.com/454597910): Implement this feature in shell_integration.
  }
#endif
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
void CompareHttpProgIdWithDefaultState(
    default_browser::DefaultBrowserState default_state,
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

  base::UmaHistogramEnumeration(
      "DefaultBrowser.HttpProgIdAssocValidationResult", result);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

namespace default_browser {

DefaultBrowserManager::ShellDelegate::~ShellDelegate() = default;

// Static
std::unique_ptr<DefaultBrowserManager::ShellDelegate>
DefaultBrowserManager::CreateDefaultDelegate() {
  return std::make_unique<ShellDelegateImpl>();
}

DefaultBrowserManager::DefaultBrowserManager(
    std::unique_ptr<ShellDelegate> shell_delegate)
    : shell_delegate_(std::move(shell_delegate)) {}

DefaultBrowserManager::~DefaultBrowserManager() = default;

// Static
std::unique_ptr<DefaultBrowserController>
DefaultBrowserManager::CreateControllerFor(
    DefaultBrowserEntrypointType entrypoint) {
  return std::make_unique<DefaultBrowserController>(
      std::make_unique<ShellIntegrationDefaultBrowserSetter>(), entrypoint);
}

void DefaultBrowserManager::GetDefaultBrowserState(
    DefaultBrowserCheckCompletionCallback callback) {
  shell_delegate_->StartCheckIsDefault(
      base::BindOnce(&DefaultBrowserManager::OnDefaultBrowserCheckResult,
                     base::Unretained(this), std::move(callback)));
}

void DefaultBrowserManager::OnDefaultBrowserCheckResult(
    default_browser::DefaultBrowserCheckCompletionCallback callback,
    default_browser::DefaultBrowserState default_state) {
#if BUILDFLAG(IS_WIN)
  // Only consider performing the secondary check for telemetry if there was a
  // definitive result on default browser state.
  if (default_state == shell_integration::IS_DEFAULT ||
      default_state == shell_integration::NOT_DEFAULT) {
    if (base::FeatureList::IsEnabled(
            default_browser::kPerformDefaultBrowserCheckValidations)) {
      // TODO(crbug.com/454597910): Perform additional checks with "https" and
      // methods other that checking program id.
      shell_delegate_->StartCheckDefaultClientProgId(
          "http",
          base::BindOnce(&CompareHttpProgIdWithDefaultState, default_state));
    }
  }
#endif  // BUILDFLAG(IS_WIN)
  std::move(callback).Run(default_state);
}

base::CallbackListSubscription
DefaultBrowserManager::RegisterDefaultBrowserChanged(
    base::RepeatingClosure callback) {
  if (!monitor_) {
    monitor_ = std::make_unique<DefaultBrowserMonitor>();
    monitor_->StartMonitor();
  }

  return monitor_->RegisterDefaultBrowserChanged(std::move(callback));
}

}  // namespace default_browser
