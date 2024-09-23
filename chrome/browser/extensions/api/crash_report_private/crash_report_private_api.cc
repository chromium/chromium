// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/crash_report_private/crash_report_private_api.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {
namespace api {

namespace {

JavaScriptErrorReport::WindowType GetWindowType(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser)
    return JavaScriptErrorReport::WindowType::kNoBrowser;
  if (!browser->app_controller())
    return JavaScriptErrorReport::WindowType::kRegularTabbed;
  if (browser->app_controller()->system_app())
    return JavaScriptErrorReport::WindowType::kSystemWebApp;
  return JavaScriptErrorReport::WindowType::kWebApp;
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

  const auto params = crash_report_private::ReportError::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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

  if (params->info.debug_id) {
    error_report.debug_id = *params->info.debug_id;
  }

  if (params->info.stack_trace) {
    error_report.stack_trace = std::move(*params->info.stack_trace);
  }

  if (web_contents) {
    error_report.window_type = GetWindowType(web_contents);

    base::TimeTicks render_process_start_time =
        web_contents->GetPrimaryMainFrame()->GetProcess()->GetLastInitTime();
    base::TimeDelta render_process_uptime;
    if (!render_process_start_time.is_null()) {
      render_process_uptime =
          base::TimeTicks::Now() - render_process_start_time;
    }
    // Note: This can be 0 in tests or if the process isn't live (implying
    // process fails to start up or terminated). Report this anyways as it can
    // hint at race conditions.
    error_report.renderer_process_uptime_ms =
        render_process_uptime.InMilliseconds();
  }

  error_report.app_locale =
      ExtensionsBrowserClient::Get()->GetApplicationLocale();

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
