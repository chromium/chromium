// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_DEFAULT_RANKING_H_
#define CHROME_BROWSER_SHARE_DEFAULT_RANKING_H_

#include <string>
#include <vector>

namespace sharing {

std::vector<std::string> DefaultRankingForLocaleAndType(
    const std::string& locale,
    const std::string& type);

}  // namespace sharing

#endif  // CHROME_BROWSER_SHARE_DEFAULT_RANKING_H_
