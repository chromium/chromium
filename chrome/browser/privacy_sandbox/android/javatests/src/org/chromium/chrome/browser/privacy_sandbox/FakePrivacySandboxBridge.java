// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Java implementation of PrivacySandboxBridge for testing.
 */
class FakePrivacySandboxBridge implements PrivacySandboxBridge.Natives {
    private boolean mIsPrivacySandboxEnabled = true;
    private final Set<String> mCurrentTopTopics = new HashSet<>(Arrays.asList("Foo", "Bar"));
    private final Set<String> mBlockedTopics =
            new HashSet<>(Arrays.asList("BlockedFoo", "BlockedBar"));

    @Override
    public boolean isPrivacySandboxEnabled() {
        return mIsPrivacySandboxEnabled;
    }

    @Override
    public boolean isPrivacySandboxManaged() {
        return false;
    }

    @Override
    public void setPrivacySandboxEnabled(boolean enabled) {
        mIsPrivacySandboxEnabled = enabled;
    }

    @Override
    public boolean isFlocEnabled() {
        return true;
    }

    @Override
    public void setFlocEnabled(boolean enabled) {
        assert false;
    }

    @Override
    public boolean isFlocIdResettable() {
        return true;
    }

    @Override
    public void resetFlocId() {
        assert false;
    }

    @Override
    public String getFlocStatusString() {
        return null;
    }

    @Override
    public String getFlocGroupString() {
        return null;
    }

    @Override
    public String getFlocUpdateString() {
        return null;
    }

    @Override
    public String getFlocDescriptionString() {
        return null;
    }

    @Override
    public String getFlocResetExplanationString() {
        return null;
    }

    @Override
    public String[] getCurrentTopTopics() {
        return mCurrentTopTopics.toArray(new String[0]);
    }

    public void setCurrentTopTopics(String... topics) {
        mCurrentTopTopics.clear();
        mCurrentTopTopics.addAll(Arrays.asList(topics));
    }

    @Override
    public String[] getBlockedTopics() {
        return mBlockedTopics.toArray(new String[0]);
    }

    public void setBlockedTopics(String... topics) {
        mBlockedTopics.clear();
        mBlockedTopics.addAll(Arrays.asList(topics));
    }

    @Override
    public void setTopicAllowed(String topic, boolean allowed) {
        if (allowed) {
            mCurrentTopTopics.add(topic);
            mBlockedTopics.remove(topic);
        } else {
            mCurrentTopTopics.remove(topic);
            mBlockedTopics.add(topic);
        }
    }

    @Override
    public int getRequiredDialogType() {
        return DialogType.NONE;
    }

    @Override
    public void dialogActionOccurred(int action) {
        return;
    }
}
