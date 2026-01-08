// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/glic_tab_observer.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/common/glic_tab_observer_android.h"
#else
#include "chrome/browser/glic/common/glic_tab_observer_impl.h"
#endif

// static
std::unique_ptr<GlicTabObserver> GlicTabObserver::Create(
    Profile* profile,
    EventCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<GlicTabObserverAndroid>(profile, std::move(callback));
#else
  return std::make_unique<GlicTabObserverImpl>(profile, std::move(callback));
#endif
}
