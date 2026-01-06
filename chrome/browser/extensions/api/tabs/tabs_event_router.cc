// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"

namespace extensions {

TabsEventRouter::TabsEventRouter(Profile* profile)
    : platform_delegate_(profile) {}

TabsEventRouter::~TabsEventRouter() = default;

}  // namespace extensions
