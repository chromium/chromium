// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TYPES_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TYPES_H_

#include "base/types/strong_alias.h"
#include "base/types/token_type.h"
#include "base/uuid.h"

namespace contextual_tasks {

// Uniquely identifies a Contextual Task.
// It is implemented as a base::StrongAlias wrapping base::Uuid to enforce C++
// type-safety and prevent mix-ups with window IDs, while retaining underlying
// base::Uuid compatibility for storage and sync layers across components.
// TODO(crbug.com/515502892): Move all task IDs to use this strongly typed
// alias.
using ContextualTaskId =
    base::StrongAlias<class ContextualTaskIdTag, base::Uuid>;

// Uniquely identifies a guest window/webview being tracked by the Contextual
// Tasks window tracker.
// It is implemented as a base::TokenType wrapping base::UnguessableToken
// because it is a virtual, run-time only bearer token used to securely
// associate guest webview closure events from the WebUI back to their
// browser-side tab.
using ContextualWindowId = base::TokenType<class ContextualWindowIdTag>;

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TYPES_H_
