// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_FEATURES_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

BASE_DECLARE_FEATURE(kWebApkShellUpdate);
extern const base::FeatureParam<int> kWebApkTargetShellVersion;

BASE_DECLARE_FEATURE(kWebApkMinShellVersion);

// When a POST Web Share Target is delivered to a TWA or WebAPK whose origin
// has no service worker registered for the share target action (e.g. a
// preinstalled TWA that has never been launched), first load the app's start
// URL so that it can register its service worker, and only send the POST once
// that service worker is activated.
BASE_DECLARE_FEATURE(kShareTargetWaitForServiceWorker);

// How long to wait for the service worker to activate, while a page of the
// app's origin is committed, before sending the share target POST anyway.
extern const base::FeatureParam<base::TimeDelta>
    kShareTargetWaitForServiceWorkerTimeout;

// The longest to wait for the service worker in total, including while a page
// of another origin is committed (e.g. while the user signs in). After this,
// the POST is sent if a page of the app's origin is committed, and is dropped
// otherwise.
extern const base::FeatureParam<base::TimeDelta>
    kShareTargetWaitForServiceWorkerMaxWait;

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_FEATURES_H_
