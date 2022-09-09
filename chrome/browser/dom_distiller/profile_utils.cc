// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_distiller/profile_utils.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/dom_distiller/lazy_dom_distiller_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/dom_distiller_viewer_source.h"
#include "components/dom_distiller/core/dom_distiller_features.h"

namespace dom_distiller {

void RegisterViewerSource(Profile* profile) {
  if (!dom_distiller::IsDomDistillerEnabled())
    return;

  LazyDomDistillerService* lazy_service =
      LazyDomDistillerService::Create(profile);

  // Set the JavaScript world ID.
  if (!DistillerJavaScriptWorldIdIsSet())
    SetDistillerJavaScriptWorldId(ISOLATED_WORLD_ID_CHROME_INTERNAL);

  content::URLDataSource::Add(
      profile, std::make_unique<DomDistillerViewerSource>(lazy_service));
}

}  // namespace dom_distiller
