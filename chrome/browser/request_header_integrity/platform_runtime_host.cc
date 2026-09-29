// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/request_header_integrity/platform_runtime_host.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/common/request_header_integrity/chrome_companero_loader.h"
#include "chrome/common/request_header_integrity/platform_runtime.mojom.h"
#include "chrome/common/request_header_integrity/platform_runtime_headers.h"
#include "chrome/common/request_header_integrity/request_header_integrity_url_loader_throttle.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "net/http/http_request_headers.h"

namespace request_header_integrity {

namespace {

// How long to wait after a completed refresh before asking the service for a
// new set of headers.
constexpr base::TimeDelta kRefreshInterval = base::Minutes(3);

// Throttles relaunch attempts. The cap is deliberately close to the refresh in
// order to recover promptly.
constexpr net::BackoffEntry::Policy kLaunchBackoffPolicy = {
    .num_errors_to_ignore = 0,
    .initial_delay_ms = base::Seconds(30).InMilliseconds(),
    .multiply_factor = 2.0,
    .jitter_factor = 0.1,
    .maximum_backoff_ms = base::Minutes(5).InMilliseconds(),
    .entry_lifetime_ms = -1,
    .always_use_initial_delay = false,
};

// How long a single ProcessHeaders() call may take before we give up on it.
// A crashed service disconnects and is handled by the disconnect handler, but
// a service that hangs without dying would otherwise leave the reply callback
// outstanding forever and silently stall every subsequent refresh.
constexpr base::TimeDelta kProcessHeadersTimeout = base::Seconds(30);

}  // namespace

PlatformRuntimeHost::PlatformRuntimeHost()
    : refresh_timer_(FROM_HERE,
                     kRefreshInterval,
                     base::BindRepeating(&PlatformRuntimeHost::Refresh,
                                         base::Unretained(this))),
      request_timeout_timer_(
          FROM_HERE,
          kProcessHeadersTimeout,
          base::BindRepeating(&PlatformRuntimeHost::OnRequestTimedOut,
                              base::Unretained(this))),
      launch_backoff_(&kLaunchBackoffPolicy) {}

PlatformRuntimeHost::~PlatformRuntimeHost() = default;

void PlatformRuntimeHost::OnComponentReady(const base::FilePath& library_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!library_path.IsAbsolute()) {
    return;
  }

  library_path_ = library_path;
  launch_backoff_.Reset();
  retry_timer_.Stop();
  refresh_timer_.Stop();
  request_timeout_timer_.Stop();
  runtime_.reset();
  service_.reset();
  Refresh();
}

void PlatformRuntimeHost::Refresh() {
  if (library_path_.empty()) {
    return;
  }

  if (!runtime_) {
    auto receiver = service_.BindNewPipeAndPassReceiver();
    if (service_launcher_) {
      service_launcher_.Run(std::move(receiver));
    } else {
      content::ServiceProcessHost::Options options;
      options.WithDisplayName(u"Platform Runtime");
#if BUILDFLAG(IS_WIN)
      // The sandboxed process cannot open the library itself, so content maps
      // it in before lockdown.
      options.WithPreloadedLibraries(
          {library_path_},
          content::ServiceProcessHostPreloadLibraries::GetPassKey());
#endif
      content::ServiceProcessHost::Launch<mojom::PlatformRuntimeService>(
          std::move(receiver), std::move(options).Pass());
    }
    service_.set_disconnect_handler(base::BindOnce(
        &PlatformRuntimeHost::HandleServiceFailure, base::Unretained(this)));
    service_->LoadLibrary(library_path_, runtime_.BindNewPipeAndPassReceiver());
    runtime_.set_disconnect_handler(base::BindOnce(
        &PlatformRuntimeHost::HandleServiceFailure, base::Unretained(this)));
  }

  // Not an actual request, so don't record the DynamicHeaderPresent histogram.
  net::HttpRequestHeaders input;
  RequestHeaderIntegrityURLLoaderThrottle::AddRequestIntegrityHeaders(
      &input, ChromeCompaneroLoader::GetInstance(),
      /*resource_type=*/std::nullopt);

  request_timeout_timer_.Reset();

  runtime_->ProcessHeaders(
      input, base::BindOnce(&PlatformRuntimeHost::OnHeadersReceived,
                            base::Unretained(this)));
}

void PlatformRuntimeHost::OnHeadersReceived(
    base::expected<net::HttpRequestHeaders, mojom::PlatformRuntimeStatus>
        result) {
  request_timeout_timer_.Stop();
  base::UmaHistogramBoolean("ComponentUpdater.PlatformRuntime.RefreshTimedOut",
                            false);
  base::UmaHistogramEnumeration(
      "ComponentUpdater.PlatformRuntime.ProcessHeadersStatus",
      result.error_or(mojom::PlatformRuntimeStatus::kSuccess));

  if (result ==
      base::unexpected(mojom::PlatformRuntimeStatus::kLibraryUnavailable)) {
    // The library failed to load or lacked the entry point during
    // LoadLibrary(). Tear down the service and wait for OnComponentReady() to
    // supply a new component rather than polling the same broken library.
    runtime_.reset();
    service_.reset();
    return;
  }

  // Clear any accumulated failure backoff now that the service replied.
  launch_backoff_.Reset();
  // Schedule the next periodic refresh.
  refresh_timer_.Reset();

  // The service is deliberately left bound. Tearing it down after every reply
  // would mean relaunching the process, and reloading the library from disk,
  // roughly every three minutes for the life of the browser.

  if (!result.has_value()) {
    // Keep serving the previous value rather than dropping it. A slightly
    // older set is no worse than none at all.
    return;
  }
  Publish(*std::move(result));
}

void PlatformRuntimeHost::OnRequestTimedOut() {
  base::UmaHistogramBoolean("ComponentUpdater.PlatformRuntime.RefreshTimedOut",
                            true);
  // Dropping the remote terminates the service process, which is the only way
  // to recover from a hang.
  HandleServiceFailure();
}

void PlatformRuntimeHost::HandleServiceFailure() {
  request_timeout_timer_.Stop();
  refresh_timer_.Stop();
  runtime_.reset();
  service_.reset();
  launch_backoff_.InformOfRequest(/*succeeded=*/false);
  retry_timer_.Start(
      FROM_HERE, launch_backoff_.GetTimeUntilRelease(),
      base::BindOnce(&PlatformRuntimeHost::Refresh, base::Unretained(this)));
}

void PlatformRuntimeHost::Publish(net::HttpRequestHeaders headers) {
  if (headers.GetHeaderVector() == headers_.GetHeaderVector()) {
    return;
  }
  headers_ = std::move(headers);
  PlatformRuntimeHeaders::GetInstance().Set(headers_);
  headers_changed_callbacks_.Notify();
}

const net::HttpRequestHeaders& PlatformRuntimeHost::headers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return headers_;
}

base::CallbackListSubscription
PlatformRuntimeHost::RegisterHeadersChangedCallback(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return headers_changed_callbacks_.Add(std::move(callback));
}

void PlatformRuntimeHost::SetHeadersForTesting(
    net::HttpRequestHeaders headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Publish(std::move(headers));
}

void PlatformRuntimeHost::SetServiceLauncherForTesting(
    ServiceLauncher launcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_launcher_ = std::move(launcher);
}

}  // namespace request_header_integrity
