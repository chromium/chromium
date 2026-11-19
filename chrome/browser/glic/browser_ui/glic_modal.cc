// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_modal.h"

#include <utility>

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/view.h"
#endif

namespace glic {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicModal, kDialogElementId);

GlicModal::Params::Params() = default;
GlicModal::Params::Params(Params&&) = default;
GlicModal::Params& GlicModal::Params::operator=(Params&&) = default;
GlicModal::Params::~Params() = default;

// static
void GlicModal::MaybeRunOnWebUICreated(
    content::NavigationHandle* navigation_handle,
    base::OnceCallback<void(content::WebUI*)>& on_webui_created) {
  if (!on_webui_created || !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  // Only the first navigation is the modal's own; it's not run for later ones
  // even if this one doesn't commit a WebUI (e.g. an error page).
  auto callback = std::move(on_webui_created);
  if (content::WebUI* web_ui =
          navigation_handle->GetRenderFrameHost()->GetWebUI()) {
    std::move(callback).Run(web_ui);
  }
}

}  // namespace glic
