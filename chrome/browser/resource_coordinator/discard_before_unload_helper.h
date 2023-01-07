// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_BEFORE_UNLOAD_HELPER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_BEFORE_UNLOAD_HELPER_H_

#include "base/functional/callback.h"

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

using HasBeforeUnloadHandlerCallback =
    base::OnceCallback<void(bool has_before_unload_handler)>;

// Determines if the given WebContents has a beforeunload handler; if not, it is
// safe to discard. This works by calling WebContents::DispatchBeforeUnload with
// |auto_cancel = true|, which prevents a beforeunload dialog from being
// displayed to the user in the case that there is a beforeunload handler. This
// should only be called from the UI thread, and the callback will similarly
// be invoked from there.
void HasBeforeUnloadHandler(content::WebContents* contents,
                            HasBeforeUnloadHandlerCallback&& callback);

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_BEFORE_UNLOAD_HELPER_H_
