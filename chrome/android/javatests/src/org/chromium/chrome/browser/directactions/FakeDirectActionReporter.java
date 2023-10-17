// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import androidx.annotation.RequiresApi;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

/**
 * Implementation of {@link DirectActionReporter} suitable for tests.
 *
 * <p>While Direct Actions are only available since Android Q, this test avoids using any type that
 * is specific to Android Q, so can run on older versions of the API. TODO(crbug.com/973781): Once
 * Chromium is built against Android Q SDK, have the test use {@code android.app.DirectAction}
 * directly.
 */
@RequiresApi(24) // for java.util.function.Consumer.
public class FakeDirectActionReporter implements DirectActionReporter {
    /** List of action definitions available to tests. */
    public final List<FakeDefinition> mActions = new ArrayList<>();

    private final Consumer<List<String>> mCallback;
    private final List<String> mDirectActions = new ArrayList<>();

    public FakeDirectActionReporter() {
        this(null);
        // When using this constructor, actions can only be accessed using getDirectActions()
    }

    public FakeDirectActionReporter(Consumer<List<String>> callback) {
        mCallback = callback;
    }

    public List<String> getDirectActions() {
        return mDirectActions;
    }

    @Override
    public void report() {
        mCallback.accept(mDirectActions);
    }

    @Override
    public DirectActionReporter.Definition addDirectAction(String actionId) {
        FakeDefinition action = new FakeDefinition(actionId);
        mActions.add(action);
        mDirectActions.add(actionId);
        return action;
    }

    /**
     * A simple action definition for testing.
     *
     * <p>TODO(crbug.com/806868): Share these fakes. There is another one in
     * chrome/android/junit/...directactions/
     */
    public static class FakeDefinition implements Definition {
        /** Action name string. */
        public final String mId;

        /** Parameter list for this action definition. */
        public List<FakeParameter> mParameters = new ArrayList<>();

        /** Result list for this action definition. */
        public List<FakeParameter> mResults = new ArrayList<>();

        FakeDefinition(String id) {
            mId = id;
        }

        @Override
        public Definition withParameter(String name, @Type int type, boolean required) {
            mParameters.add(new FakeParameter(name, type, required));
            return this;
        }

        @Override
        public Definition withResult(String name, @Type int type) {
            mResults.add(new FakeParameter(name, type, true));
            return this;
        }
    }

    /** A simple parameter definition for testing. */
    public static class FakeParameter {
        /** Pamater name string. */
        public final String mName;

        /** Parameter type. */
        @Type public final int mType;

        /** Whether the parameter is required or not. */
        public final boolean mRequired;

        FakeParameter(String name, @Type int type, boolean required) {
            mName = name;
            mType = type;
            mRequired = required;
        }
    }
}
