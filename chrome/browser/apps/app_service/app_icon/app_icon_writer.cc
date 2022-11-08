// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/app_icon_writer.h"

#include "chrome/browser/profiles/profile.h"

namespace apps {

AppIconWriter::AppIconWriter(Profile* profile) : profile_(profile) {}

AppIconWriter::~AppIconWriter() = default;

}  // namespace apps
