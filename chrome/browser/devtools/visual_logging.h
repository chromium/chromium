// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_VISUAL_LOGGING_H_
#define CHROME_BROWSER_DEVTOOLS_VISUAL_LOGGING_H_

#include <cstdint>
#include <vector>

struct VisualElementImpression {
  int64_t id = -1;
  int type = -1;
  int64_t parent = -1;
  int context = -1;
  int width = -1;
  int height = -1;
};

struct ImpressionEvent {
  ImpressionEvent();
  ~ImpressionEvent();
  std::vector<VisualElementImpression> impressions;
};

struct ResizeEvent {
  int64_t veid = -1;
  int width = -1;
  int height = -1;
};

struct ClickEvent {
  int64_t veid = -1;
  int mouse_button = -1;
  int context = -1;
  int double_click = -1;
};

struct HoverEvent {
  int64_t veid = -1;
  int time = -1;
  int context = -1;
};

struct DragEvent {
  int64_t veid = -1;
  int distance = -1;
  int context = -1;
};

struct ChangeEvent {
  int64_t veid = -1;
  int context = -1;
};

struct KeyDownEvent {
  int64_t veid = -1;
  int context = -1;
};

#endif  // CHROME_BROWSER_DEVTOOLS_VISUAL_LOGGING_H_
