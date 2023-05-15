// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_CONCEPT_H_
#define ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_CONCEPT_H_

#include <array>
#include <string>

#include "ash/webui/personalization_app/search/search.mojom.h"

namespace ash::personalization_app {

struct SearchConcept {
  // The id of this search concept. Used for metrics.
  mojom::SearchConceptId id;

  // The identifier for the string displayed to the user.
  int message_id;

  // Alternate message ids that map to this concept. There is a maximum of 20
  // alternate message ids, but there may be fewer. Stop reading alternate
  // message ids upon encountering a default-initialized (ie 0) value.
  std::array<int, 20> alternate_message_ids;
  // The relative url, including query parameters, to open in Personalization
  // App.
  std::string relative_url;
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_SEARCH_SEARCH_CONCEPT_H_
