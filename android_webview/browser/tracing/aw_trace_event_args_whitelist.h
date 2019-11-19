// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACE_EVENT_ARGS_WHITELIST_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACE_EVENT_ARGS_WHITELIST_H_

#include <string>

#include "base/trace_event/trace_event_impl.h"

namespace android_webview {

// Used to filter trace event arguments against a whitelist of events that
// have been manually vetted to not include any PII.
bool IsTraceEventArgsWhitelisted(
    const char* category_group_name,
    const char* event_name,
    base::trace_event::ArgumentNameFilterPredicate* arg_name_filter);

// Used to filter metadata events that have been manually vetted to not include
// any PII.
bool IsTraceMetadataWhitelisted(const std::string& name);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACE_EVENT_ARGS_WHITELIST_H_
