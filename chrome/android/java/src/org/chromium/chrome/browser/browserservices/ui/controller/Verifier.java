// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller;

import androidx.annotation.Nullable;

import org.chromium.base.Promise;

/**
 * A Delegate for the {@link CurrentPageVerifier} that provides implementation specific to
 * Trusted Web Activities, WebAPKs or A2HS as appropriate.
 */
public interface Verifier {
    /**
     * Checks whether the given URL is verified.
     *
     * The returned Promise may be immediately fulfilled (eg if we know that the given url is one
     * we shouldn't even attempt to verify or if we have a cached result). It is worth explicitly
     * checking for this to deal with the result synchronously and not incur the delay of
     * {@link Promise#then}.
     */
    Promise<Boolean> verify(String url);

    /**
     * A synchronous version of verify that returns true iff verification has previously been
     * completed successfully for the given url.
     */
    boolean wasPreviouslyVerified(String url);

    /**
     * Returns the widest scope for which verification is relevant. This can be used to determine
     * whether two different urls are the same for the purposes of verification. Returns
     * {@link null} if the given url cannot be parsed.
     *
     * The purpose of this method is to determine whether two different pages can share verification
     * state. Eg, if we've already verified a TWA for https://www.example.com/webapp/page1.html we
     * don't need to perform verification again for https://www.example.com/webapp/folder/page2.html
     * (but we do for https://developers.google.com/web/updates).
     *
     * eg, for a TWA where verification is on a per origin basis, this method would map to origins:
     * https://www.example.com/webapp/page1.html        -> https://www.example.com
     * https://www.example.com/webapp/folder/page2.html -> https://www.example.com
     * https://developers.google.com/web/updates        -> https://developers.google.com
     *
     * eg, say we have a WebAPK with the verified scope being https://www.example.com/webapp/, then
     * this method would map:
     *
     * https://www.example.com/webapp/page1.html         -> https://www.example.com/webapp/
     * https://www.example.com/webapp/folder/page2.html  -> https://www.example.com/webapp/
     * https://www.example.com/somewhere_else/page3.html -> https://www.example.com/somewhere_else/
     *
     * The last result can really be anything other than https://www.example.com/webapp/ - just
     * something to signify we aren't on the verified scope.
     */
    @Nullable
    String getVerifiedScope(String url);

    /**
     * Whether a navigation to the given URL should stay within Chrome even if there are other apps
     * on the user's device that can handle them.
     *
     * When in a TWA/WebAPK/etc we are already in an Android app specialized for the verified
     * origin, don't allow other apps to steal a navigation to the verified origin.
     */
    boolean shouldIgnoreExternalIntentHandlers(String url);
}
