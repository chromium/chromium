// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_VISUAL_LOGGING_H_
#define CHROME_BROWSER_DEVTOOLS_VISUAL_LOGGING_H_

#include <vector>

struct VisualElementImpression {
  int id = -1;
  int type = -1;
  int parent = -1;
  int context = -1;
};

struct ImpressionEvent {
  ImpressionEvent();
  ~ImpressionEvent();
  std::vector<VisualElementImpression> impressions;
};

struct ClickEvent {
  int veid = -1;
  int mouse_button = -1;
  int context = -1;
};

struct HoverEvent {
  int veid = -1;
  int time = -1;
  int context = -1;
};

struct DragEvent {
  int veid = -1;
  int distance = -1;
  int context = -1;
};

struct ChangeEvent {
  int veid = -1;
  int context = -1;
};

struct KeyDownEvent {
  int veid = -1;
  int context = -1;
};

#endif  // CHROME_BROWSER_DEVTOOLS_VISUAL_LOGGING_H_
