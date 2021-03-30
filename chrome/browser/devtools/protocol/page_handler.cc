// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/page_handler.h"

#include "components/subresource_filter/content/browser/devtools_interaction_tracker.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "ui/gfx/image/image.h"

PageHandler::PageHandler(content::WebContents* web_contents,
                         protocol::UberDispatcher* dispatcher)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
  protocol::Page::Dispatcher::wire(dispatcher, this);
}

PageHandler::~PageHandler() {
  ToggleAdBlocking(false /* enabled */);
}

void PageHandler::ToggleAdBlocking(bool enabled) {
  if (!web_contents())
    return;

  // Create the DevtoolsInteractionTracker lazily (note that this call is a
  // no-op if the object was already created).
  subresource_filter::DevtoolsInteractionTracker::CreateForWebContents(
      web_contents());

  subresource_filter::DevtoolsInteractionTracker::FromWebContents(
      web_contents())
      ->ToggleForceActivation(enabled);
}

protocol::Response PageHandler::Enable() {
  enabled_ = true;
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::Disable() {
  enabled_ = false;
  ToggleAdBlocking(false /* enable */);
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response PageHandler::SetAdBlockingEnabled(bool enabled) {
  if (!enabled_)
    return protocol::Response::ServerError("Page domain is disabled.");
  ToggleAdBlocking(enabled);
  return protocol::Response::Success();
}

void PageHandler::GetInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback) {
  auto errors = std::make_unique<protocol::Array<std::string>>();
  webapps::InstallableManager* manager =
      web_contents()
          ? webapps::InstallableManager::FromWebContents(web_contents())
          : nullptr;
  if (!manager) {
    callback->sendFailure(
        protocol::Response::ServerError("Unable to fetch errors for target"));
    return;
  }
  manager->GetAllErrors(base::BindOnce(&PageHandler::GotInstallabilityErrors,
                                       std::move(callback)));
}

// static
void PageHandler::GotInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback,
    std::vector<content::InstallabilityError> installability_errors) {
  auto result_installability_errors =
      std::make_unique<protocol::Array<protocol::Page::InstallabilityError>>();
  for (const auto& installability_error : installability_errors) {
    auto installability_error_arguments = std::make_unique<
        protocol::Array<protocol::Page::InstallabilityErrorArgument>>();
    for (const auto& error_argument :
         installability_error.installability_error_arguments) {
      installability_error_arguments->emplace_back(
          protocol::Page::InstallabilityErrorArgument::Create()
              .SetName(error_argument.name)
              .SetValue(error_argument.value)
              .Build());
    }
    result_installability_errors->emplace_back(
        protocol::Page::InstallabilityError::Create()
            .SetErrorId(installability_error.error_id)
            .SetErrorArguments(std::move(installability_error_arguments))
            .Build());
  }
  callback->sendSuccess(
      std::move(result_installability_errors));
}

void PageHandler::GetManifestIcons(
    std::unique_ptr<GetManifestIconsCallback> callback) {
  webapps::InstallableManager* manager =
      web_contents()
          ? webapps::InstallableManager::FromWebContents(web_contents())
          : nullptr;

  if (!manager) {
    callback->sendFailure(
        protocol::Response::ServerError("Unable to fetch icons for target"));
    return;
  }

  manager->GetPrimaryIcon(
      base::BindOnce(&PageHandler::GotManifestIcons, std::move(callback)));
}

void PageHandler::GotManifestIcons(
    std::unique_ptr<GetManifestIconsCallback> callback,
    const SkBitmap* primary_icon) {
  protocol::Maybe<protocol::Binary> primaryIconAsBinary;

  if (primary_icon && !primary_icon->empty()) {
    primaryIconAsBinary = std::move(protocol::Binary::fromRefCounted(
        gfx::Image::CreateFrom1xBitmap(*primary_icon).As1xPNGBytes()));
  }

  callback->sendSuccess(std::move(primaryIconAsBinary));
}
