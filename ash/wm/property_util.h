// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PROPERTY_UTIL_H_
#define ASH_WM_PROPERTY_UTIL_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

namespace aura {
class PropertyConverter;
class Window;
}  // namespace aura

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace ui {
namespace mojom {
enum class WindowType;
}
}  // namespace ui

namespace ash {

// Functions for extracting properties that are used at a Window creation time.
// Clients pass an initial set of properties when requesting a new aura::Window.
// Not all of these properties need be persisted, some are used solely to
// configure the window. The functions below extract those properties.

// Long lived properties are converted and stored as properties on the
// associated aura::Window. See aura::PropertyConverter for this set of
// properties.

using InitProperties = std::map<std::string, std::vector<uint8_t>>;

// Returns the kDisplayId_InitProperty if present, otherwise
// kInvalidDisplayID.
int64_t GetInitialDisplayId(const InitProperties& properties);

// If |window| has the |kContainerId_InitProperty| set as a property, then
// the value of |kContainerId_InitProperty| is set in |container_id| and true
// is returned. Otherwise false is returned.
bool GetInitialContainerId(const InitProperties& properties, int* container_id);

bool GetInitialBounds(const InitProperties& properties, gfx::Rect* bounds);

bool GetWindowPreferredSize(const InitProperties& properties, gfx::Size* size);

bool ShouldRemoveStandardFrame(const InitProperties& properties);

// Applies |properties| to |window| using |property_converter|.
void ApplyProperties(
    aura::Window* window,
    aura::PropertyConverter* property_converter,
    const std::map<std::string, std::vector<uint8_t>>& properties);

}  // namespace ash

#endif  // ASH_WM_PROPERTY_UTIL_H_
