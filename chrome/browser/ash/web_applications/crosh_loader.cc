// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/crosh_loader.h"

#include "chrome/browser/ash/web_applications/terminal_source.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/url_data_source.h"

CroshLoader::CroshLoader(Profile* profile) {
  content::URLDataSource::Add(profile, TerminalSource::ForCrosh(profile));
}

CroshLoader::~CroshLoader() = default;
