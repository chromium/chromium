// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_GLIC_NAVIGATION_H_
#define CHROME_BROWSER_GLIC_COMMON_GLIC_NAVIGATION_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace content {
class NavigationHandle;
}  // namespace content

struct NavigateParams;

namespace glic {

// Navigates according to the configuration specified in |params|.
// Takes ownership of |params|.
base::WeakPtr<content::NavigationHandle> Navigate(
    std::unique_ptr<NavigateParams> params);

// Navigates according to the configuration specified in |params|
// asynchronously. Takes ownership of |params|.
void NavigateAsync(
    std::unique_ptr<NavigateParams> params,
    base::OnceCallback<void(base::WeakPtr<content::NavigationHandle>)>
        callback);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_GLIC_NAVIGATION_H_
