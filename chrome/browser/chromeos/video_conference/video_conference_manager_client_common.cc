// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"

#include <string>

#include "base/containers/contains.h"

namespace video_conference {

const char* kSkipAppIds[3] = {
    "behllobkkfkfnphdnhnkndlbkcpglgmj",  // TestExtensionID
    "mecfefiddjlmabpeilblgegnbioikfmp",  // SigninProfileTestExtensionID
    "feedback",  // Chrome Feedback tool chrome://feedback/.
};

bool ShouldSkipId(const std::string& id) {
  return base::Contains(kSkipAppIds, id);
}

}  // namespace video_conference
