// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reader_mode/profile_utils.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reader_mode/lazy_reader_mode_service.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "components/web_distiller/content/browser/distiller_javascript_utils.h"
#include "components/web_distiller/content/browser/web_distiller_viewer_source.h"
#include "components/web_distiller/core/web_distiller_features.h"

namespace reader_mode {

void RegisterViewerSource(Profile* profile) {
  if (!web_distiller::IsWebDistillerEnabled()) {
    return;
  }

  LazyReaderModeService* lazy_service = LazyReaderModeService::Create(profile);

  // Set the JavaScript world ID.
  if (!web_distiller::DistillerJavaScriptWorldIdIsSet()) {
    web_distiller::SetDistillerJavaScriptWorldId(
        ISOLATED_WORLD_ID_CHROME_INTERNAL);
  }

  content::URLDataSource::Add(
      profile,
      std::make_unique<web_distiller::WebDistillerViewerSource>(lazy_service));
}

}  // namespace reader_mode
