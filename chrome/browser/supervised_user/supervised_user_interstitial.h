// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_

#include <memory>
#include <string>

#include "base/allocator/partition_allocator/pointers/raw_ref.h"
#include "base/functional/callback_forward.h"
#include "components/supervised_user/core/browser/supervised_user_error_page.h"
#include "url/gurl.h"

namespace supervised_user {
class WebContentHandler;
}

class PrefService;
class SupervisedUserService;

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
      std::unique_ptr<supervised_user::WebContentHandler> web_content_handler,
      SupervisedUserService& supervised_user_service,
      const GURL& url,
      supervised_user::FilteringBehaviorReason reason);

  static std::string GetHTMLContents(
      SupervisedUserService* supervised_user_service,
      PrefService* pref_service,
      supervised_user::FilteringBehaviorReason reason,
      bool already_sent_request,
      bool is_main_frame);

  void GoBack();
  void RequestUrlAccessRemote(base::OnceCallback<void(bool)> callback);
  void RequestUrlAccessLocal(base::OnceCallback<void(bool)> callback);
  void ShowFeedback();

  // Getter methods.
  const GURL& url() const { return url_; }
  supervised_user::WebContentHandler* web_content_handler() {
    return web_content_handler_.get();
  }

 private:
  SupervisedUserInterstitial(
      std::unique_ptr<supervised_user::WebContentHandler> web_content_handler,
      SupervisedUserService& supervised_user_service,
      const GURL& url,
      supervised_user::FilteringBehaviorReason reason);
  void OutputRequestPermissionSourceMetric();

  const raw_ref<SupervisedUserService> supervised_user_service_;

  std::unique_ptr<supervised_user::WebContentHandler> web_content_handler_;

  // The last committed url for this frame.
  GURL url_;
  supervised_user::FilteringBehaviorReason reason_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_INTERSTITIAL_H_
