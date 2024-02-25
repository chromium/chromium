// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/context_menu_content_type_web_view.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

using extensions::Extension;
using extensions::ProcessManager;

ContextMenuContentTypeWebView::ContextMenuContentTypeWebView(
    const base::WeakPtr<extensions::WebViewGuest> web_view_guest,
    const content::ContextMenuParams& params)
    : ContextMenuContentType(params, true),
      web_view_guest_(std::move(web_view_guest)) {}

ContextMenuContentTypeWebView::~ContextMenuContentTypeWebView() {
}

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
    case ITEM_GROUP_LINK:
    case ITEM_GROUP_SEARCHWEBFORIMAGE:
    case ITEM_GROUP_SEARCH_PROVIDER:
    case ITEM_GROUP_PRINT:
    case ITEM_GROUP_ALL_EXTENSION:
    case ITEM_GROUP_PRINT_PREVIEW:
      return false;
    case ITEM_GROUP_CURRENT_EXTENSION:
      // Show contextMenus API items.
      return true;
    case ITEM_GROUP_DEVELOPER:
      {
      const extensions::Extension* embedder_extension = GetExtension();
      if (chrome::GetChannel() >= version_info::Channel::DEV) {
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

        // TODO(lazyboy): Enable this for mac too when http://crbug.com/380405
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
