// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_COMPOSE_AX_SERIALIZATION_UTILS_H_
#define CHROME_BROWSER_COMPOSE_COMPOSE_AX_SERIALIZATION_UTILS_H_

namespace ui {
struct AXTreeUpdate;
}  // namespace ui

namespace optimization_guide::proto {
class AXTreeUpdate;
}  // namespace optimization_guide::proto

// A class containing utils helpful for AX tree serialization.
class ComposeAXSerializationUtils {
 public:
  // Populate the AXTreeUpdate proto structure from the ui structure.
  static void PopulateAXTreeUpdate(
      const ui::AXTreeUpdate& source,
      optimization_guide::proto::AXTreeUpdate* destination);
};

#endif  // CHROME_BROWSER_COMPOSE_COMPOSE_AX_SERIALIZATION_UTILS_H_
