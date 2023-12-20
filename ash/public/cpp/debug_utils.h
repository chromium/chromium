// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DEBUG_UTILS_H_
#define ASH_PUBLIC_CPP_DEBUG_UTILS_H_

#include <sstream>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace ash {
namespace debug {

class ASH_EXPORT DebugWindowHierarchyDelegate {
 public:
  virtual ~DebugWindowHierarchyDelegate() = default;
  virtual std::vector<raw_ptr<aura::Window, VectorExperimental>>
  GetAdjustedWindowChildren(aura::Window* window) const = 0;

  virtual std::vector<raw_ptr<ui::Layer, VectorExperimental>>
  GetAdjustedLayerChildren(const ui::Layer* layer) const = 0;
};

ASH_EXPORT void SetDebugWindowHierarchyDelegate(
    std::unique_ptr<DebugWindowHierarchyDelegate> delegate);

// Prints all windows layer hierarchy to |out|.
ASH_EXPORT void PrintLayerHierarchy(std::ostringstream* out);

// Prints current active window's view hierarchy to |out|.
ASH_EXPORT void PrintViewHierarchy(std::ostringstream* out);

// Prints all windows hierarchy to |out|. If |scrub_data| is true, we
// may skip some data fields that are not very important for debugging. Returns
// a list of window titles. Window titles will be removed from |out| if
// |scrub_data| is true.
ASH_EXPORT std::vector<std::string> PrintWindowHierarchy(
    std::ostringstream* out,
    bool scrub_data);

}  // namespace debug
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DEBUG_UTILS_H_
