// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_CONCEPT_H_
#define ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_CONCEPT_H_

#include <string>

namespace ash {
namespace personalization_app {

struct SearchConcept {
  // The identifier for the string displayed to the user.
  int message_id;
  // The relative url, including query parameters, to open in Personalization
  // App.
  std::string relative_url;
};

}  // namespace personalization_app
}  // namespace ash

#endif  // ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_CONCEPT_H_
