// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_BACKGROUND_TRACING_FIELD_TRIAL_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_BACKGROUND_TRACING_FIELD_TRIAL_H_

namespace android_webview {

// Sets up background tracing in system mode if configured. Does not
// require the metrics service to be enabled.
// Since only one of the tracing modes (system/preemptive/reactive) is
// specified in a given config, if system tracing is set up as a result
// of this method call, any calls to MaybeSetupWebViewOnlyTracing() in
// the same browser process will do nothing. Returns true if background
// tracing is successfully initialized, false otherwise.
bool MaybeSetupSystemTracingFromFieldTrial();

// Sets up app-only background tracing if configured. Requires the metrics
// service to be enabled.
// Since only one of the tracing modes (system/preemptive/reactive) is
// specified in a given config, if app-only tracing is set up as a
// result of this method call, any calls to MaybeSetupSystemTracing() in
// the same browser process will do nothing. Returns true if background
// tracing is successfully initialized, false otherwise.
bool MaybeSetupWebViewOnlyTracingFromFieldTrial();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_BACKGROUND_TRACING_FIELD_TRIAL_H_
