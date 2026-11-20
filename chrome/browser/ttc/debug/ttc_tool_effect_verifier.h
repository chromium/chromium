// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_DEBUG_TTC_TOOL_EFFECT_VERIFIER_H_
#define CHROME_BROWSER_TTC_DEBUG_TTC_TOOL_EFFECT_VERIFIER_H_

#include <stdint.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class TickClock;
}

namespace ttc {

// Checks that a TTC tool call had its expected, visible effect on the tab it
// acted on, for the debug tools. An actor tool can report success while
// acting on a tab the user can't see; this catches that.
//
// Only navigations that start after the verifier is created are judged, so
// that a navigation already in flight isn't attributed to the tool.
class TtcToolEffectVerifier final : public content::WebContentsObserver {
 public:
  enum class Expectation {
    // A primary main-frame, cross-document navigation commits, isn't an error
    // page, started on or redirected through the requested host (if any), in
    // a visible tab. Same-document navigations (fragments, pushState) are
    // ignored.
    kNavigation,
    // Like kNavigation, but same-document navigations count too: going back
    // or forward in a single-page app doesn't load a new document.
    kHistoryNavigation,
    // The tab is closed.
    kTabClosed,
  };

  // How long to wait for the effect.
  static constexpr base::TimeDelta kTimeout = base::Seconds(10);

  // `verified` is false if the effect wasn't observed; `detail` says what was
  // observed. The callback may delete the verifier.
  using ResultCallback =
      base::OnceCallback<void(bool verified, std::string detail)>;

  // Returns what to verify for `tool_name` called with `arguments`, or nullopt
  // if its effect isn't checked. open_url is only checked when it loads the
  // URL in its tab rather than a new one. Browser-level tools such as
  // open_known_page, which may switch to another tab instead of navigating,
  // aren't checked.
  static std::optional<Expectation> ExpectationForTool(
      std::string_view tool_name,
      const base::DictValue& arguments);

  // Returns the host a navigation is expected to land on for `tool_name` with
  // `arguments`, if known.
  static std::optional<std::string> ExpectedHost(
      std::string_view tool_name,
      const base::DictValue& arguments);

  TtcToolEffectVerifier(content::WebContents* contents,
                        Expectation expectation,
                        std::optional<std::string> expected_host,
                        base::TimeDelta timeout,
                        const base::TickClock* clock,
                        ResultCallback callback);
  TtcToolEffectVerifier(const TtcToolEffectVerifier&) = delete;
  TtcToolEffectVerifier& operator=(const TtcToolEffectVerifier&) = delete;
  ~TtcToolEffectVerifier() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

 private:
  bool ExpectsNavigation() const;
  void OnTimeout();
  void Finish(bool verified, std::string detail);

  const Expectation expectation_;
  const std::optional<std::string> expected_host_;
  const base::TimeDelta timeout_;
  base::OneShotTimer timer_;
  // Primary main-frame navigations that started after construction.
  std::set<int64_t> navigation_ids_;
  ResultCallback callback_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_DEBUG_TTC_TOOL_EFFECT_VERIFIER_H_
