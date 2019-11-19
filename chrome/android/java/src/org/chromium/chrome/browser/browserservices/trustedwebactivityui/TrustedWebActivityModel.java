// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.ui.modelutil.PropertyModel;

import javax.inject.Inject;

/**
 * Model describing the state of a Trusted Web Activity.
 */
@ActivityScope
public class TrustedWebActivityModel extends PropertyModel {

    /** The state of Trusted Web Activity disclosure. Can be one of the constants below. */
    public static final WritableIntPropertyKey DISCLOSURE_STATE =
            new WritableIntPropertyKey();

    public static final int DISCLOSURE_STATE_NOT_SHOWN = 0;
    public static final int DISCLOSURE_STATE_SHOWN = 1;
    public static final int DISCLOSURE_STATE_DISMISSED_BY_USER = 2;

    /** Callback for routing disclosure-related view events back to controller side. */
    public static final WritableObjectPropertyKey<DisclosureEventsCallback>
            DISCLOSURE_EVENTS_CALLBACK = new WritableObjectPropertyKey<>();

    public interface DisclosureEventsCallback {
        /** Called when user accepted the disclosure. */
        void onDisclosureAccepted();
    }

    @Inject
    public TrustedWebActivityModel() {
        super(DISCLOSURE_STATE, DISCLOSURE_EVENTS_CALLBACK);
    }
}
