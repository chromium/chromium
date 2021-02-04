// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/crash_report_private/crash_report_private_api.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/renderer_uptime_tracker.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace extensions {
namespace api {

namespace {

WindowType GetWindowType(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return WindowType::kNoBrowser;
  if (!browser->app_controller())
    return WindowType::kRegularTabbed;
  if (browser->app_controller()->is_for_system_web_app())
    return WindowType::kSystemWebApp;
  return WindowType::kWebApp;
}

}  // namespace

CrashReportPrivateReportErrorFunction::CrashReportPrivateReportErrorFunction() =
    default;

CrashReportPrivateReportErrorFunction::
    ~CrashReportPrivateReportErrorFunction() = default;

ExtensionFunction::ResponseAction CrashReportPrivateReportErrorFunction::Run() {
  content::WebContents* web_contents = GetSenderWebContents();
  // Silently drop the crash report if devtools has ever been opened for this
  // |web_contents|.
  if (web_contents && content::DevToolsAgentHost::HasFor(web_contents)) {
    return RespondNow(NoArguments());
  }

  // TODO(https://crbug.com/986166): Use crash_reporter for Chrome OS.
  const auto params = crash_report_private::ReportError::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  auto processor = JsErrorReportProcessor::Get();
  if (!processor) {
    VLOG(3) << "No processor for error report";
    return RespondNow(Error("No processor for error report"));
  }

  JavaScriptErrorReport error_report;
  error_report.message = std::move(params->info.message);
  error_report.url = std::move(params->info.url);
  error_report.source_system =
      JavaScriptErrorReport::SourceSystem::kCrashReportApi;
  if (params->info.product) {
    error_report.product = std::move(*params->info.product);
  }

  if (params->info.version) {
    error_report.version = std::move(*params->info.version);
  }

  if (params->info.line_number) {
    error_report.line_number = *params->info.line_number;
  }

  if (params->info.column_number) {
    error_report.column_number = *params->info.column_number;
  }

  if (params->info.stack_trace) {
    error_report.stack_trace = std::move(*params->info.stack_trace);
  }

  if (web_contents) {
    error_report.window_type = GetWindowType(web_contents);

    if (web_contents->GetMainFrame() &&
        web_contents->GetMainFrame()->GetProcess()) {
      int pid = web_contents->GetMainFrame()->GetProcess()->GetID();
      base::TimeDelta render_process_uptime =
          metrics::RendererUptimeTracker::Get()->GetProcessUptime(pid);
      // Note: This can be 0 in tests or if the process can't be found (implying
      // process fails to start up or terminated). Report this anyways as it can
      // hint at race conditions.
      error_report.renderer_process_uptime_ms =
          render_process_uptime.InMilliseconds();
    }
  }

  error_report.app_locale = g_browser_process->GetApplicationLocale();

  processor->SendErrorReport(
      std::move(error_report),
      base::BindOnce(&CrashReportPrivateReportErrorFunction::OnReportComplete,
                     this),
      browser_context());

  return RespondLater();
}

void CrashReportPrivateReportErrorFunction::OnReportComplete() {
  Respond(NoArguments());
}

}  // namespace api
}  // namespace extensions
