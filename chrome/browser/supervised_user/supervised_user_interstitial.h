// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
class SupervisedUserFaviconRequestHandler;
#endif

// This class is used by SupervisedUserNavigationObserver to handle requests
// from supervised user error page. The error page is shown when a page is
// blocked because it is on a denylist (in "allow everything" mode), not on any
// allowlist (in "allow only specified sites" mode), or doesn't pass safe
// search.
class SupervisedUserInterstitial {
 public:
  // The names of histograms emitted by this class.
  static constexpr char kInterstitialCommandHistogramName[] =
      "ManagedMode.BlockingInterstitialCommand";
  static constexpr char kInterstitialPermissionSourceHistogramName[] =
      "ManagedUsers.RequestPermissionSource";

  // For use in the kInterstitialCommandHistogramName histogram.
  //
  // The enum values should remain synchronized with the enum
  // ManagedModeBlockingCommand in tools/metrics/histograms/enums.xml.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Commands {
    // PREVIEW = 0,
    BACK = 1,
    // NTP = 2,
    REMOTE_ACCESS_REQUEST = 3,
    LOCAL_ACCESS_REQUEST = 4,
    HISTOGRAM_BOUNDING_VALUE = 5
  };

  // For use in the kInterstitialPermissionSourceHistogramName histogram.
  //
  // The enum values should remain synchronized with the
  // enum ManagedUserURLRequestPermissionSource in
  // tools/metrics/histograms/enums.xml.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RequestPermissionSource {
    MAIN_FRAME = 0,
    SUB_FRAME,
    HISTOGRAM_BOUNDING_VALUE
  };

  SupervisedUserInterstitial(const SupervisedUserInterstitial&) = delete;
  SupervisedUserInterstitial& operator=(const SupervisedUserInterstitial&) =
      delete;

  ~SupervisedUserInterstitial();

  static std::unique_ptr<SupervisedUserInterstitial> Create(
      content::WebContents* web_contents,
      const GURL& url,
      supervised_user::FilteringBehaviorReason reason,
      int frame_id,
      int64_t interstitial_navigation_id);

  static std::string GetHTMLContents(
      Profile* profile,
      supervised_user::FilteringBehaviorReason reason,
      bool already_sent_request,
      bool is_main_frame);

  void GoBack();
  void RequestUrlAccessRemote(base::OnceCallback<void(bool)> callback);
  void RequestUrlAccessLocal(base::OnceCallback<void(bool)> callback);
  void ShowFeedback();

  // Getter methods.
  content::WebContents* web_contents() { return web_contents_; }
  int frame_id() const { return frame_id_; }
  int64_t interstitial_navigation_id() const {
    return interstitial_navigation_id_;
  }
  const GURL& url() const { return url_; }

 private:
  SupervisedUserInterstitial(content::WebContents* web_contents,
                             const GURL& url,
                             supervised_user::FilteringBehaviorReason reason,
                             int frame_id,
                             int64_t interstitial_navigation_id);

  // Tries to go back.
  void AttemptMoveAwayFromCurrentFrameURL();

  void OnInterstitialDone();

  void OutputRequestPermissionSourceMetric();

  // Owns SupervisedUserNavigationObserver which owns us.
  raw_ptr<content::WebContents> web_contents_;

  raw_ptr<Profile> profile_;

  // The last committed url for this frame.
  GURL url_;
  supervised_user::FilteringBehaviorReason reason_;

  // The uniquely identifying global id for the frame.
  int frame_id_;

  // The Navigation ID of the navigation that last triggered the interstitial.
  int64_t interstitial_navigation_id_;

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<SupervisedUserFaviconRequestHandler> favicon_handler_;
#endif
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
