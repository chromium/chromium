// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/keyword_ranker.h"

namespace app_list {

KeywordRanker::KeywordRanker() = default;

KeywordRanker::~KeywordRanker() = default;

void KeywordRanker::Start(const std::u16string& query,
                          ResultsMap& results,
                          CategoriesList& categories) {
  // TODO(b/263059094): when the user start input, this function will
  // be called.
}

void KeywordRanker::UpdateResultRanks(ResultsMap& results,
                                      ProviderType provider) {
  // TODO(b/263059094): update the result by boost the scores that
  // match certain keywords, the rest remain unchanged
}

}  // namespace app_list
