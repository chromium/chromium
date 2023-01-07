// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_UNHANDLED_TAP_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_UNHANDLED_TAP_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace contextual_search {

typedef base::RepeatingCallback<void(int x_px, int y_px)> UnhandledTapCallback;

// Binds a Mojo unhandled-tap notifier message-handler to the frame host
// observed by this observer.
class UnhandledTapWebContentsObserver
    : public content::WebContentsUserData<UnhandledTapWebContentsObserver> {
 public:
  // Creates an observer for the given |web_contents| that binds a Mojo request
  // for an endpoint to the UnhandledTapNotifier service.  This will create an
  // instance of the contextual_search::CreateUnhandledTapNotifierImpl to handle
  // those messages.
  explicit UnhandledTapWebContentsObserver(content::WebContents* web_contents);

  UnhandledTapWebContentsObserver(const UnhandledTapWebContentsObserver&) =
      delete;
  UnhandledTapWebContentsObserver& operator=(
      const UnhandledTapWebContentsObserver&) = delete;

  ~UnhandledTapWebContentsObserver() override;

  void set_device_scale_factor(float factor) { device_scale_factor_ = factor; }

  float device_scale_factor() const { return device_scale_factor_; }

  void set_unhandled_tap_callback(UnhandledTapCallback callback) {
    unhandled_tap_callback_ = callback;
  }

  UnhandledTapCallback unhandled_tap_callback() const {
    return unhandled_tap_callback_;
  }

 private:
  friend class content::WebContentsUserData<UnhandledTapWebContentsObserver>;

  // Scale factor to convert from dips to pixels for tap coordinates when
  // calling back through the given |unhandled_tap_callback_|.
  float device_scale_factor_;
  UnhandledTapCallback unhandled_tap_callback_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_search

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_UNHANDLED_TAP_WEB_CONTENTS_OBSERVER_H_
