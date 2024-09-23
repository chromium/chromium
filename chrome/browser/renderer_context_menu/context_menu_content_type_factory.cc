// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/url_constants.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/guest_view/web_view/context_menu_content_type_web_view.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_app_mode.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_extension_popup.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_platform_app.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/session_manager/core/session_manager.h"
#endif

namespace {

bool IsUserSessionBlocked() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (session_manager::SessionManager::Get() &&
      session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return true;
  }
#endif
  return false;
}

// Context menu content with no supported groups.
class NullContextMenuContentType : public ContextMenuContentType {
 public:
  explicit NullContextMenuContentType(const content::ContextMenuParams& params)
      : ContextMenuContentType(params, false) {}

  NullContextMenuContentType(const NullContextMenuContentType&) = delete;
  NullContextMenuContentType& operator=(const NullContextMenuContentType&) =
      delete;

  ~NullContextMenuContentType() override = default;

  bool SupportsGroup(int group) override { return false; }
};

}  // namespace

ContextMenuContentTypeFactory::ContextMenuContentTypeFactory() {
}

ContextMenuContentTypeFactory::~ContextMenuContentTypeFactory() {
}

// static.
std::unique_ptr<ContextMenuContentType> ContextMenuContentTypeFactory::Create(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  if (IsUserSessionBlocked())
    return std::make_unique<NullContextMenuContentType>(params);

  return CreateInternal(render_frame_host, params);
}

// static
std::unique_ptr<ContextMenuContentType>
ContextMenuContentTypeFactory::CreateInternal(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (IsRunningInForcedAppMode()) {
    return base::WrapUnique(new ContextMenuContentTypeAppMode(params));
  }

  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromRenderFrameHost(render_frame_host);
  if (web_view_guest) {
    return base::WrapUnique(new ContextMenuContentTypeWebView(
        web_view_guest->GetWeakPtr(), params));
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  const extensions::mojom::ViewType view_type =
      extensions::GetViewType(web_contents);

  if (view_type == extensions::mojom::ViewType::kAppWindow) {
    return base::WrapUnique(
        new ContextMenuContentTypePlatformApp(web_contents, params));
  }

  if (view_type == extensions::mojom::ViewType::kExtensionPopup) {
    return base::WrapUnique(new ContextMenuContentTypeExtensionPopup(params));
  }

#endif

  return std::make_unique<ContextMenuContentType>(params, true);
}
