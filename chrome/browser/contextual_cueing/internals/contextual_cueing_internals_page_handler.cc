// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/internals/contextual_cueing_internals_page_handler.h"

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace contextual_cueing_internals {

ContextualCueingInternalsPageHandler::ContextualCueingInternalsPageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

ContextualCueingInternalsPageHandler::~ContextualCueingInternalsPageHandler() =
    default;

void ContextualCueingInternalsPageHandler::GetShownCues(
    GetShownCuesCallback callback) {
  contextual_cueing::ContextualCueingService* service =
      contextual_cueing::ContextualCueingServiceFactory::GetForProfile(
          profile_);
  if (service) {
    std::vector<mojom::CueLogPtr> cues;
    for (const auto& cue : service->shown_cues()) {
      cues.push_back(cue->Clone());
    }
    std::move(callback).Run(std::move(cues));
  } else {
    std::move(callback).Run({});
  }
}

}  // namespace contextual_cueing_internals
