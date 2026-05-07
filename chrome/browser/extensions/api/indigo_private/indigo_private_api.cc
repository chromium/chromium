// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/indigo_private/indigo_private_api.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
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

IndigoPrivateGetOriginalImageFunction::IndigoPrivateGetOriginalImageFunction() =
    default;

IndigoPrivateGetOriginalImageFunction::
    ~IndigoPrivateGetOriginalImageFunction() = default;

ExtensionFunction::ResponseAction IndigoPrivateGetOriginalImageFunction::Run() {
  indigo::IndigoImageReplacement* replacement =
      GetImageReplacement(render_frame_host());
  if (!replacement) {
    return RespondNow(Error("No image replacement found"));
  }

  std::vector<uint8_t> image_bytes = replacement->TakeOriginalImageWebpBytes();
  if (image_bytes.empty()) {
    return RespondNow(Error("Original image not available"));
  }

  api::indigo_private::ImageData result;
  api::indigo_private::ImageData::Value value;
  value.as_binary = std::move(image_bytes);
  result.value = std::move(value);
  return RespondNow(ArgumentList(
      api::indigo_private::GetOriginalImage::Results::Create(result)));
}

IndigoPrivateGetReplacementImageFunction::
    IndigoPrivateGetReplacementImageFunction() = default;

IndigoPrivateGetReplacementImageFunction::
    ~IndigoPrivateGetReplacementImageFunction() = default;

ExtensionFunction::ResponseAction
IndigoPrivateGetReplacementImageFunction::Run() {
  indigo::IndigoImageReplacement* replacement =
      GetImageReplacement(render_frame_host());
  if (!replacement) {
    return RespondNow(Error("No image replacement found"));
  }

  GURL url = replacement->TakeReplacementImageURL();
  if (!url.is_empty()) {
    api::indigo_private::ImageData result;
    api::indigo_private::ImageData::Value value;
    value.as_string = url.spec();
    result.value = std::move(value);
    return RespondNow(ArgumentList(
        api::indigo_private::GetReplacementImage::Results::Create(result)));
  }

  if (!replacement->SetPendingReplacementImageCallback(
          base::BindOnce(&IndigoPrivateGetReplacementImageFunction::
                             OnReplacementImageAvailable,
                         this))) {
    return RespondNow(Error("Already waiting for replacement image"));
  }
  return RespondLater();
}

void IndigoPrivateGetReplacementImageFunction::OnReplacementImageAvailable(
    GURL replacement_image_url) {
  if (!browser_context()) {
    return;
  }
  if (replacement_image_url.is_empty()) {
    Respond(Error("Image replacement cancelled"));
    return;
  }
  api::indigo_private::ImageData result;
  api::indigo_private::ImageData::Value value;
  value.as_string = replacement_image_url.spec();
  result.value = std::move(value);
  Respond(ArgumentList(
      api::indigo_private::GetReplacementImage::Results::Create(result)));
}

}  // namespace extensions
