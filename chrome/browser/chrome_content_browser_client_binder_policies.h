// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_BINDER_POLICIES_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_BINDER_POLICIES_H_

#include "content/public/browser/mojo_binder_policy_map.h"

// Intended to be called only by
// ChromeContentBrowserClient::RegisterMojoBinderPoliciesForSameOriginPrerendering().
// It is in its own file so that security review can be required by the OWNERS
// file.
void RegisterChromeMojoBinderPoliciesForSameOriginPrerendering(
    content::MojoBinderPolicyMap& policy_map);

// Intended to be called only by
// ChromeContentBrowserClient::RegisterMojoBinderPoliciesForPreview().
// It is in its own file so that security review can be required by the OWNERS
// file.
void RegisterChromeMojoBinderPoliciesForPreview(
    content::MojoBinderPolicyMap& policy_map);

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_BINDER_POLICIES_H_
