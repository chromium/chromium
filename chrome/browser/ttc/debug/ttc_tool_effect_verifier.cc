// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_tool_effect_verifier.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace ttc {

namespace {

std::string_view StripWww(std::string_view host) {
  return base::StartsWith(host, "www.") ? host.substr(4) : host;
}

// Whether `host` is `expected` or a subdomain of it, ignoring "www.".
bool HostMatches(std::string_view host, std::string_view expected) {
  host = StripWww(host);
  expected = StripWww(expected);
  return host == expected ||
         (host.size() > expected.size() && base::EndsWith(host, expected) &&
          host[host.size() - expected.size() - 1] == '.');
}

// Whether any URL `navigation_handle` went through is on `expected`. A
// navigation that starts on the requested host may redirect to another one
// (sign-in, short links, gmail.com to mail.google.com) and is still the
// tool's effect.
bool PassedThroughHost(content::NavigationHandle* navigation_handle,
                       std::string_view expected) {
  return std::ranges::any_of(navigation_handle->GetRedirectChain(),
                             [expected](const GURL& url) {
                               return HostMatches(url.host(), expected);
                             });
}

}  // namespace

// static
std::optional<TtcToolEffectVerifier::Expectation>
TtcToolEffectVerifier::ExpectationForTool(std::string_view tool_name,
                                          const base::DictValue& arguments) {
  if (tool_name == "open_url") {
    const std::optional<bool> new_tab = arguments.FindBool("new_tab");
    return new_tab.has_value() && !*new_tab
               ? std::make_optional(Expectation::kNavigation)
               : std::nullopt;
  }
  if (tool_name == "perform_search" || tool_name == "reload_page") {
    return Expectation::kNavigation;
  }
  if (tool_name == "go_back" || tool_name == "go_forward") {
    return Expectation::kHistoryNavigation;
  }
  if (tool_name == "close_tab") {
    return Expectation::kTabClosed;
  }
  return std::nullopt;
}

// static
std::optional<std::string> TtcToolEffectVerifier::ExpectedHost(
    std::string_view tool_name,
    const base::DictValue& arguments) {
  if (tool_name != "open_url") {
    return std::nullopt;
  }
  const std::string* url = arguments.FindString("url");
  if (!url) {
    return std::nullopt;
  }
  GURL gurl(*url);
  if (!gurl.is_valid() || gurl.host().empty()) {
    return std::nullopt;
  }
  return std::string(gurl.host());
}

TtcToolEffectVerifier::TtcToolEffectVerifier(
    content::WebContents* contents,
    Expectation expectation,
    std::optional<std::string> expected_host,
    base::TimeDelta timeout,
    const base::TickClock* clock,
    ResultCallback callback)
    : content::WebContentsObserver(contents),
      expectation_(expectation),
      expected_host_(std::move(expected_host)),
      timeout_(timeout),
      timer_(clock),
      callback_(std::move(callback)) {
  timer_.Start(FROM_HERE, timeout_,
               base::BindOnce(&TtcToolEffectVerifier::OnTimeout,
                              base::Unretained(this)));
}

TtcToolEffectVerifier::~TtcToolEffectVerifier() = default;

bool TtcToolEffectVerifier::ExpectsNavigation() const {
  return expectation_ == Expectation::kNavigation ||
         expectation_ == Expectation::kHistoryNavigation;
}

void TtcToolEffectVerifier::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (ExpectsNavigation() && navigation_handle->IsInPrimaryMainFrame()) {
    navigation_ids_.insert(navigation_handle->GetNavigationId());
  }
}

void TtcToolEffectVerifier::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Navigations that were already in flight when the tool ran aren't its
  // effect.
  if (!ExpectsNavigation() ||
      !navigation_ids_.erase(navigation_handle->GetNavigationId())) {
    return;
  }
  // A fragment or pushState change on the page isn't the effect of a tool
  // that loads a page, but it is how history moves in single-page apps.
  if (navigation_handle->IsSameDocument() &&
      expectation_ != Expectation::kHistoryNavigation) {
    return;
  }
  const GURL& url = navigation_handle->GetURL();
  if (!navigation_handle->HasCommitted()) {
    // Typically an unrelated navigation superseded by the tool's own; keep
    // waiting, the timeout catches the case where nothing commits.
    if (navigation_handle->GetNetErrorCode() == net::ERR_ABORTED) {
      return;
    }
    Finish(false,
           base::StrCat(
               {"navigation to ", url.spec(), " didn't commit (",
                net::ErrorToShortString(navigation_handle->GetNetErrorCode()),
                ")"}));
    return;
  }
  if (navigation_handle->IsErrorPage()) {
    Finish(false,
           base::StrCat(
               {"committed an error page for ", url.spec(), " (",
                net::ErrorToShortString(navigation_handle->GetNetErrorCode()),
                ")"}));
    return;
  }
  if (expected_host_ &&
      !PassedThroughHost(navigation_handle, *expected_host_)) {
    Finish(false, base::StrCat({"committed ", url.spec(), " but expected host ",
                                *expected_host_}));
    return;
  }
  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    Finish(false, base::StrCat({"committed ", url.spec(),
                                " in a tab that isn't visible"}));
    return;
  }
  Finish(true, base::StrCat({"committed ", url.spec(), " in a visible tab"}));
}

void TtcToolEffectVerifier::WebContentsDestroyed() {
  if (expectation_ == Expectation::kTabClosed) {
    Finish(true, "tab closed");
  } else {
    Finish(false, "the tab was closed before the navigation committed");
  }
}

void TtcToolEffectVerifier::OnTimeout() {
  const std::string seconds =
      base::StringPrintf("%.0f s", timeout_.InSecondsF());
  if (expectation_ == Expectation::kTabClosed) {
    Finish(false, base::StrCat({"the tab is still open after ", seconds}));
    return;
  }
  const std::string url =
      web_contents() ? web_contents()->GetLastCommittedURL().spec() : "";
  Finish(false, base::StrCat({"no navigation committed within ", seconds,
                              "; the tab is still on ", url}));
}

void TtcToolEffectVerifier::Finish(bool verified, std::string detail) {
  if (!callback_) {
    return;
  }
  timer_.Stop();
  Observe(nullptr);
  // May delete `this`.
  std::move(callback_).Run(verified, std::move(detail));
}

}  // namespace ttc
