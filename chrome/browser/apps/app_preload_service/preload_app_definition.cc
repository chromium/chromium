// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

namespace apps {

std::string PreloadAppDefinition::GetName() const {
  return app_proto_.app_group().name();
}

std::ostream& operator<<(std::ostream& os, const PreloadAppDefinition& app) {
  os << "- Name: " << app.GetName();
  return os;
}

}  // namespace apps
