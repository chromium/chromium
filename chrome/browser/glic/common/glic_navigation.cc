// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_navigation.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/navigation_handle.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#endif

namespace glic {

namespace {

void EnsureBrowserForNavigateParams(NavigateParams* params) {
#if BUILDFLAG(IS_ANDROID)
  if (!params->browser) {
    BrowserWindowInterface* last_active_browser = nullptr;
    ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
        [&](BrowserWindowInterface* browser) {
          if (browser->GetProfile() == params->initiating_profile) {
            last_active_browser = browser;
            return false;  // stop iterating once found
          }
          return true;  // keep iterating
        });
    params->browser = last_active_browser;
  }
#endif
}

}  // namespace

base::WeakPtr<content::NavigationHandle> Navigate(
    std::unique_ptr<NavigateParams> params) {
  EnsureBrowserForNavigateParams(params.get());
  return ::Navigate(params.get());
}

void NavigateAsync(
    std::unique_ptr<NavigateParams> params,
    base::OnceCallback<void(base::WeakPtr<content::NavigationHandle>)>
        callback) {
  EnsureBrowserForNavigateParams(params.get());
  NavigateParams* params_ptr = params.get();
  // We use BindOnce with a lambda that takes the unique_ptr by value to bind
  // it to the internal callback and ensure it lives until ::Navigate completes.
  ::Navigate(
      params_ptr,
      base::BindOnce(
          [](std::unique_ptr<NavigateParams> kept_alive_params,
             base::OnceCallback<void(base::WeakPtr<content::NavigationHandle>)>
                 original_callback,
             base::WeakPtr<content::NavigationHandle> handle) {
            if (original_callback) {
              std::move(original_callback).Run(handle);
            }
          },
          std::move(params), std::move(callback)));
}

}  // namespace glic
