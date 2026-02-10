// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/context_menu_content_type_web_view.h"

#include <optional>

#include "base/command_line.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

using extensions::Extension;
using extensions::ProcessManager;

namespace {
bool IsContextualTaskWebUIHost(
    base::WeakPtr<extensions::WebViewGuest> web_view_guest) {
  if (!web_view_guest || !web_view_guest->owner_rfh()) {
    return false;
  }
  const GURL& url =
      web_view_guest->owner_rfh()->GetMainFrame()->GetLastCommittedURL();
  return url.scheme() == content::kChromeUIScheme &&
         url.host() == chrome::kChromeUIContextualTasksHost;
}
}  // namespace

// static
std::optional<version_info::Channel>
    ContextMenuContentTypeWebView::channel_override_ = std::nullopt;

ContextMenuContentTypeWebView::ContextMenuContentTypeWebView(
    const base::WeakPtr<extensions::WebViewGuest> web_view_guest,
    const content::ContextMenuParams& params)
    : ContextMenuContentType(params, true),
      web_view_guest_(std::move(web_view_guest)) {}

ContextMenuContentTypeWebView::~ContextMenuContentTypeWebView() = default;

const Extension* ContextMenuContentTypeWebView::GetExtension() const {
  if (!web_view_guest_)
    return nullptr;

  ProcessManager* process_manager = ProcessManager::Get(
      web_view_guest_->GetGuestMainFrame()->GetBrowserContext());
  return process_manager->GetExtensionForRenderFrameHost(
      web_view_guest_->GetGuestMainFrame());
}

bool ContextMenuContentTypeWebView::SupportsGroup(int group) {
  switch (group) {
    case ITEM_GROUP_PAGE:
    case ITEM_GROUP_FRAME:
    case ITEM_GROUP_SEARCHWEBFORIMAGE:
    case ITEM_GROUP_SEARCH_PROVIDER:
    case ITEM_GROUP_PRINT:
    case ITEM_GROUP_ALL_EXTENSION:
    case ITEM_GROUP_PRINT_PREVIEW:
      return false;
    case ITEM_GROUP_LINK: {
      // Enable links context menu items for contextual tasks WebUI page, which
      // has a webview embedding an external URL.
      // TODO(crbug.com/470110425): Support more menu items for contextual tasks
      // webview if needed.
      if (IsContextualTaskWebUIHost(web_view_guest_)) {
        return ContextMenuContentType::SupportsGroup(group);
      }
      return false;
    }
    case ITEM_GROUP_CURRENT_EXTENSION:
      // Show contextMenus API items.
      return true;
    case ITEM_GROUP_DEVELOPER:
      {
      // Contextual Tasks is embedding an external URL, and as such needs
      // to be allowed to use the developer tools for the embedded page.
      if (IsContextualTaskWebUIHost(web_view_guest_)) {
        return ContextMenuContentType::SupportsGroup(group);
      }
      const extensions::Extension* embedder_extension = GetExtension();
      if (GetChannel() >= version_info::Channel::DEV) {
        // Hide dev tools items in guests inside WebUI if we are not running
        // canary or tott.
        // Note that this check might not be sufficient to hide dev tools
        // items on OS_MAC if we start supporting <webview> inside
        // component extensions.
        // For a list of places where <webview>/GuestViews are supported, see:
        // https://goo.gl/xfJkwp.
        if (!embedder_extension && web_view_guest_ &&
            web_view_guest_->owner_rfh()->GetMainFrame()->GetWebUI()) {
          return false;
        }
      }

      // TODO(lazyboy): Enable this for mac too when http://crbug.com/41111850
      // is fixed.
#if !BUILDFLAG(IS_MAC)
        // Add dev tools for unpacked extensions.
        return !embedder_extension ||
               extensions::Manifest::IsUnpackedLocation(
                   embedder_extension->location()) ||
               base::CommandLine::ForCurrentProcess()->HasSwitch(
                   switches::kDebugPackedApps);
#else
        return ContextMenuContentType::SupportsGroup(group);
#endif
      }
    default:
      return ContextMenuContentType::SupportsGroup(group);
  }
}

// static
void ContextMenuContentTypeWebView::SetChannelForTesting(  // IN-TEST
    std::optional<version_info::Channel> channel) {
  channel_override_ = channel;
}

// static
version_info::Channel ContextMenuContentTypeWebView::GetChannel() {
  if (channel_override_.has_value()) {
    return *channel_override_;
  }
  return chrome::GetChannel();
}
