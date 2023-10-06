// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_VISUAL_LOGGING_H_
#define CHROME_BROWSER_DEVTOOLS_VISUAL_LOGGING_H_

#include <vector>

struct VisualElementImpression {
  int id = 0;
  int type = 0;
  int parent = 0;
  int context = 0;
};

struct ImpressionEvent {
  ImpressionEvent();
  ~ImpressionEvent();
  std::vector<VisualElementImpression> impressions;
};

struct ClickEvent {
  int veid = 0;
  int mouse_button = 0;
  int context = 0;
};

struct ChangeEvent {
  int veid = 0;
  int context = 0;
};

struct KeyDownEvent {
  int veid = 0;
  int context = 0;
};

#endif  // CHROME_BROWSER_DEVTOOLS_VISUAL_LOGGING_H_
