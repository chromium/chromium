// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_FUTURE_BROWSER_FEATURES_H_
#define CHROME_BROWSER_GLIC_COMMON_FUTURE_BROWSER_FEATURES_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#endif

// The intent is to remove functions from this file as things become available
// on Android.
namespace glic {

inline base::CallbackListSubscription RegisterDidBecomeActive(
    BrowserWindowInterface* browser_window,
    base::RepeatingCallback<void(BrowserWindowInterface*)> callback) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  return browser_window->RegisterDidBecomeActive(std::move(callback));
#else
  return base::CallbackListSubscription();
#endif
}

inline base::CallbackListSubscription RegisterDidBecomeInactive(
    BrowserWindowInterface* browser_window,
    base::RepeatingCallback<void(BrowserWindowInterface*)> callback) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  return browser_window->RegisterDidBecomeInactive(std::move(callback));
#else
  return base::CallbackListSubscription();
#endif
}

inline base::CallbackListSubscription RegisterBrowserDidClose(
    BrowserWindowInterface* browser_window,
    base::RepeatingCallback<void(BrowserWindowInterface*)> callback) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  return browser_window->RegisterBrowserDidClose(std::move(callback));
#else
  return base::CallbackListSubscription();
#endif
}

inline base::CallbackListSubscription RegisterActiveTabDidChange(
    BrowserWindowInterface* browser_window,
    base::RepeatingCallback<void(BrowserWindowInterface*)> callback) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  return browser_window->RegisterActiveTabDidChange(std::move(callback));
#else
  return base::CallbackListSubscription();
#endif
}

inline bool IsActive(BrowserWindowInterface* browser_window) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  return browser_window->IsActive();
#else
  return true;
#endif
}

inline bool IsDeleteScheduled(BrowserWindowInterface* browser_window) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  return browser_window->GetBrowserForMigrationOnly()->is_delete_scheduled();
#else
  return false;
#endif
}

inline tabs::TabInterface* GetActiveTabInterface(
    BrowserWindowInterface* browser_window) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  return browser_window ? browser_window->GetActiveTabInterface() : nullptr;
#else
  return nullptr;
#endif
}

inline base::WeakPtr<BrowserWindowInterface> GetBrowserWindowInterfaceWeakPtr(
    BrowserWindowInterface* browser_window) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  return browser_window ? browser_window->GetWeakPtr() : nullptr;
#else
  return nullptr;
#endif
}

inline base::WeakPtr<content::NavigationHandle> DoNavigate(
    NavigateParams* params) {
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  return Navigate(params);
#else
  return nullptr;
#endif
}

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_FUTURE_BROWSER_FEATURES_H_
