// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_DEBUG_COMMANDS_H_
#define ASH_ACCELERATORS_DEBUG_COMMANDS_H_

#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_export.h"

// This file contains implementations of commands that are used only when
// debugging.
//
// NOTE: these commands may be enabled in about:flags, so that they may be
// available at run time.
namespace ash {
namespace debug {

// Print the views::View, ui::Layer and aura::Window hierarchies. This may be
// useful in debugging user reported bugs.
ASH_EXPORT void PrintUIHierarchies();

// Returns true if debug accelerators are enabled.
ASH_EXPORT bool DebugAcceleratorsEnabled();

// Returns true if developer accelerators are enabled.
ASH_EXPORT bool DeveloperAcceleratorsEnabled();

// Performs |action| if |action| belongs to a debug-only accelerator and debug
// accelerators are enabled.
ASH_EXPORT void PerformDebugActionIfEnabled(AcceleratorAction action);

}  // namespace debug
}  // namespace ash

#endif  // ASH_ACCELERATORS_DEBUG_COMMANDS_H_
