// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_ANDROID_H_

#include "chrome/browser/ui/android/tab_model/tab_model.h"

namespace url_param_filter {

// The equivalent of CrossOtrObserver for Android "Open in Incognito".
// The difference is only in the condition in which the observer is added;
// on desktop, this is based on fields in NavigateParams, but the TabLaunch type
// is used on Android.
void MaybeCreateCrossOtrObserverForTabLaunchType(
    content::WebContents* web_contents,
    const TabModel::TabLaunchType type);

}  // namespace url_param_filter
#endif  // CHROME_BROWSER_ANDROID_URL_PARAM_FILTER_CROSS_OTR_OBSERVER_ANDROID_H_
