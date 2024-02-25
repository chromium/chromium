// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_UTILS_H_
#define CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_UTILS_H_

class GURL;

enum class PopupProvider {
  kUnknown = 0,
  kGoogle = 1,
};

PopupProvider GetPopupProvider(const GURL& popup_url);

#endif  // CHROME_BROWSER_TPCD_HEURISTICS_OPENER_HEURISTIC_UTILS_H_
