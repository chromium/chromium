// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_frame_host.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/common/url_constants.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_config_map.h"
#include "extensions/browser/extension_config_map_factory.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/error_reporting/error_reporting_util.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"
#endif  // !BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ChromeExtensionWebContentsObserver::ChromeExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : ExtensionWebContentsObserver(web_contents),
      content::WebContentsUserData<ChromeExtensionWebContentsObserver>(
          *web_contents) {}

ChromeExtensionWebContentsObserver::~ChromeExtensionWebContentsObserver() =
    default;

// static
void ChromeExtensionWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  content::WebContentsUserData<
      ChromeExtensionWebContentsObserver>::CreateForWebContents(web_contents);

  // Initialize this instance if necessary.
  FromWebContents(web_contents)->Initialize();
}

std::unique_ptr<ExtensionFrameHost>
ChromeExtensionWebContentsObserver::CreateExtensionFrameHost(
    content::WebContents* web_contents) {
  return std::make_unique<ChromeExtensionFrameHost>(web_contents);
}

void ChromeExtensionWebContentsObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(initialized());
  ReloadIfTerminated(render_frame_host);
  ExtensionWebContentsObserver::RenderFrameCreated(render_frame_host);
}

#if !BUILDFLAG(IS_ANDROID)
void ChromeExtensionWebContentsObserver::OnExtensionJsError(
    content::RenderFrameHost* source_frame,
    const Extension& extension,
    const std::u16string& message,
    int32_t line_no,
    const GURL& url,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  JavaScriptErrorReport report;
  report.message = base::UTF16ToUTF8(message);
  report.line_number = line_no;
  report.url = RedactUrlForErrorReports(url);
  report.source_system =
      JavaScriptErrorReport::SourceSystem::kExtensionObserver;
  if (untrusted_stack_trace) {
    report.stack_trace = base::UTF16ToUTF8(*untrusted_stack_trace);
  }

  GURL page_url = source_frame->GetLastCommittedURL();
  if (page_url.is_valid()) {
    report.page_url = RedactUrlForErrorReports(page_url);
  }

  if (!version_info::IsOfficialBuild() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableCrashOnComponentExtensionJsError)) {
    auto* config_map =
        ExtensionConfigMapFactory::GetForBrowserContext(browser_context());
    auto* provider =
        config_map ? config_map->GetConfigProvider(extension) : nullptr;
    if (provider && provider->ShouldCrashOnJsErrorInDevelopmentBuild()) {
      // LOG(FATAL) will crash the browser in development builds.
      LOG(FATAL)
          << "JavaScript error in component extension development build. "
             "To disable this crash, use --"
          << switches::kDisableCrashOnComponentExtensionJsError << ".\n"
          << "URL: " << report.url << "\n"
          << "Page URL: " << report.page_url.value_or("(empty)") << "\n"
          << "Message: " << report.message << "\n"
          << "Stack trace: " << report.stack_trace.value_or("(empty)");
    }
  }

  scoped_refptr<JsErrorReportProcessor> processor =
      JsErrorReportProcessor::Get();
  if (!processor) {
    // This usually means we are not on an official Google build.
    return;
  }

  processor->SendErrorReport(std::move(report), base::DoNothing(),
                             browser_context());
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ChromeExtensionWebContentsObserver::InitializeRenderFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(initialized());
  ExtensionWebContentsObserver::InitializeRenderFrame(render_frame_host);
  WindowController* controller = dispatcher()->GetExtensionWindowController();
  if (controller) {
    GetLocalFrame(render_frame_host)
        ->UpdateBrowserWindowId(controller->GetWindowId());
  }
}

void ChromeExtensionWebContentsObserver::ReloadIfTerminated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(initialized());
  std::string extension_id = util::GetExtensionIdFromFrame(render_frame_host);
  if (extension_id.empty()) {
    return;
  }

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  // Reload the extension if it has crashed.
  // TODO(yoz): This reload doesn't happen synchronously for unpacked
  //            extensions. It seems to be fast enough, but there is a race.
  //            We should delay loading until the extension has reloaded.
  if (registry->terminated_extensions().GetByID(extension_id)) {
    ExtensionRegistrar::Get(browser_context())->ReloadExtension(extension_id);
  }
}

void ChromeExtensionWebContentsObserver::SetUpRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ExtensionWebContentsObserver::SetUpRenderFrameHost(render_frame_host);

  // This logic should match
  // ChromeContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories.
  const Extension* extension = GetExtensionFromFrame(render_frame_host, false);
  if (!extension) {
    return;
  }

  int process_id = render_frame_host->GetProcess()->GetDeprecatedID();
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();

  // Components of chrome that are implemented as extensions or platform apps
  // are allowed to use chrome://resources/ and chrome://theme/ URLs.
  if ((extension->is_extension() || extension->is_platform_app()) &&
      Manifest::IsComponentLocation(extension->location())) {
    policy->GrantRequestOrigin(
        process_id, url::Origin::Create(GURL(blink::kChromeUIResourcesURL)));
    policy->GrantRequestOrigin(
        process_id, url::Origin::Create(GURL(chrome::kChromeUIThemeURL)));
  }

  // Extensions, legacy packaged apps, and component platform apps are allowed
  // to use chrome://favicon/ and chrome://extension-icon/ URLs. Hosted apps are
  // not allowed because they are served via web servers (and are generally
  // never given access to Chrome APIs).
  if (extension->is_extension() || extension->is_legacy_packaged_app() ||
      (extension->is_platform_app() &&
       Manifest::IsComponentLocation(extension->location()))) {
    policy->GrantRequestOrigin(
        process_id, url::Origin::Create(GURL(chrome::kChromeUIFaviconURL)));
    policy->GrantRequestOrigin(
        process_id,
        url::Origin::Create(GURL(chrome::kChromeUIExtensionIconURL)));
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeExtensionWebContentsObserver);

}  // namespace extensions
