// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/indigo_private/indigo_private_api.h"

#include "chrome/browser/indigo/indigo_image_replacement.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/common/extensions/api/indigo_private.h"
#include "content/public/browser/render_frame_host.h"

namespace extensions {

namespace {
indigo::IndigoImageReplacement* GetImageReplacement(
    content::RenderFrameHost* rfh) {
  if (!rfh) {
    return nullptr;
  }

  content::Page& page = rfh->GetPage();
  indigo::IndigoImageReplacementManager* manager =
      indigo::IndigoImageReplacementManager::GetForPage(page);
  if (!manager) {
    return nullptr;
  }

  return manager->GetImageReplacementForFrame(*rfh);
}
}  // namespace

IndigoPrivateReadyToRenderFunction::IndigoPrivateReadyToRenderFunction() =
    default;

IndigoPrivateReadyToRenderFunction::~IndigoPrivateReadyToRenderFunction() =
    default;

ExtensionFunction::ResponseAction IndigoPrivateReadyToRenderFunction::Run() {
  indigo::IndigoImageReplacement* replacement =
      GetImageReplacement(render_frame_host());
  if (!replacement) {
    return RespondNow(Error("No image replacement found"));
  }

  replacement->OnReadyToRender();
  return RespondNow(NoArguments());
}

}  // namespace extensions
