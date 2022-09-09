// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CUSTOMTABS_CUSTOM_TAB_SESSION_STATE_TRACKER_H_
#define CHROME_BROWSER_ANDROID_CUSTOMTABS_CUSTOM_TAB_SESSION_STATE_TRACKER_H_

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "third_party/metrics_proto/custom_tab_session.pb.h"

namespace chrome {
namespace android {

// This is a singleton.
class CustomTabSessionStateTracker {
 public:
  static CustomTabSessionStateTracker& GetInstance();

  CustomTabSessionStateTracker(const CustomTabSessionStateTracker&) = delete;
  CustomTabSessionStateTracker& operator=(const CustomTabSessionStateTracker&) =
      delete;

  void RecordCustomTabSession(int64_t time_sec,
                              std::string package_name,
                              int32_t session_duration,
                              bool was_user_closed,
                              bool is_partial);

  bool HasCustomTabSessionState() const;
  std::unique_ptr<metrics::CustomTabSessionProto> GetSession();
  void OnUserInteraction();

 private:
  friend class base::NoDestructor<CustomTabSessionStateTracker>;

  CustomTabSessionStateTracker();
  ~CustomTabSessionStateTracker();

  bool has_custom_tab_session_;
  bool did_user_interact_;

  std::unique_ptr<metrics::CustomTabSessionProto> custom_tab_session_;
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CUSTOMTABS_CUSTOM_TAB_SESSION_STATE_TRACKER_H_
