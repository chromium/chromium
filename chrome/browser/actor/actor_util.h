// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_
#define CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_

namespace content {
class WebContents;
class BrowserContext;
}  // namespace content

namespace actor {

// Returns true if an actor is currently operating on the given WebContents.
bool IsActorOperatingOnWebContents(content::BrowserContext* context,
                                   content::WebContents* wc);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_UTIL_H_
