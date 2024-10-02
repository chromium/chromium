// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_ENTERPRISE_HELPER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_ENTERPRISE_HELPER_H_

#include "base/functional/callback_forward.h"
namespace android_webview::enterprise {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview.enterprise
enum class EnterpriseState { kUnknown, kEnterpriseOwned, kNotOwned };

using EnterpriseStateCallback = base::OnceCallback<void(EnterpriseState)>;

// Attempts to determine if the embedding app is running in an
// enterprise-managed context (either a wholly-owned enterprise device or a
// work profile). This can only be done reliably on Android T and higher, but
// work profiles can also be detected from Android R.
//
// A result of `EnterpriseState::kUnknown` means that the method was unable to
// rule out that the context is enterprise owned.
//
// This method will post the check to a background thread. The callback will
// be posted back on the calling sequence.
void GetEnterpriseState(EnterpriseStateCallback callback);
}  // namespace android_webview::enterprise

#endif  // ANDROID_WEBVIEW_BROWSER_AW_ENTERPRISE_HELPER_H_
