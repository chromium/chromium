// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_METRICS_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_METRICS_H_

#include <string>
#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_setter.h"

namespace default_browser {

std::string UiEntrypointTypeToString(
    DefaultBrowserEntrypointType ui_entrypoint);
std::string SetterTypeToString(DefaultBrowserSetterType setter_type);

// Returns "DefaultBrowser.<entrypoint>.<setter>.<metric_type>".

std::string GetEntrypointHistogramName(
    DefaultBrowserEntrypointType entrypoint_type,
    DefaultBrowserSetterType setter_type,
    std::string_view metric_type);

// Records the result of a setter run started from `ui_entrypoint`. `duration`
// is only recorded on success.
void RecordSetterResult(DefaultBrowserEntrypointType ui_entrypoint,
                        DefaultBrowserSetterType setter_type,
                        bool success,
                        base::TimeDelta duration);

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_METRICS_H_
