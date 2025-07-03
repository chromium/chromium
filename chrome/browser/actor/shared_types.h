// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SHARED_TYPES_H_
#define CHROME_BROWSER_ACTOR_SHARED_TYPES_H_

#include "chrome/common/actor.mojom-data-view.h"
namespace actor {

using MouseClickType = mojom::ClickAction_Type;
using MouseClickCount = mojom::ClickAction_Count;

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SHARED_TYPES_H_
