// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.ui.modelutil.PropertyModel;

import javax.inject.Inject;

/** Model describing the state of a Trusted Web Activity. */
@ActivityScope
public class TrustedWebActivityModel extends PropertyModel {
    /** The state of Trusted Web Activity disclosure. Can be one of the constants below. */
    public static final WritableIntPropertyKey DISCLOSURE_STATE = new WritableIntPropertyKey();

    public static final int DISCLOSURE_STATE_NOT_SHOWN = 0;
    public static final int DISCLOSURE_STATE_SHOWN = 1;
    public static final int DISCLOSURE_STATE_DISMISSED_BY_USER = 2;

    /** The webpage scope that the disclosure is valid for, it may be displayed to the user. */
    public static final WritableObjectPropertyKey<String> DISCLOSURE_SCOPE =
            new WritableObjectPropertyKey<>();

    /**
     * Whether this is the first time the disclosure has been shown.
     * This determines the priority of the notification disclosure.
     * Only valid when DISCLOSURE_STATE == DISCLOSURE_STATE_SHOWN.
     */
    public static final WritableBooleanPropertyKey DISCLOSURE_FIRST_TIME =
            new WritableBooleanPropertyKey();

    /** Callback for routing disclosure-related view events back to controller side. */
    public static final WritableObjectPropertyKey<DisclosureEventsCallback>
            DISCLOSURE_EVENTS_CALLBACK = new WritableObjectPropertyKey<>();

    /** The name of the package that is running the TWA. */
    public static final WritableObjectPropertyKey<String> PACKAGE_NAME =
            new WritableObjectPropertyKey<>();

    /** A callback for when the disclosure is accepted. */
    public interface DisclosureEventsCallback {
        /** Called when user accepted the disclosure. */
        void onDisclosureAccepted();

        /** Called when the disclosure is shown. */
        void onDisclosureShown();
    }

    @Inject
    public TrustedWebActivityModel() {
        super(
                DISCLOSURE_STATE,
                DISCLOSURE_FIRST_TIME,
                DISCLOSURE_SCOPE,
                DISCLOSURE_EVENTS_CALLBACK,
                PACKAGE_NAME);
    }
}
