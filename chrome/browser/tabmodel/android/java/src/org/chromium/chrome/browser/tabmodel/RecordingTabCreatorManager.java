// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;

/**
 * A {@link TabCreatorManager} that delegates to another {@link TabCreatorManager} while
 * orchestrating the recording the properties of tabs created by the delegate. This replaces the
 * need for using the live {@link TabModel} when diffing tab state stores.
 */
@NullMarked
public class RecordingTabCreatorManager implements TabCreatorManager {
    private final TabCreatorManager mDelegate;
    private @Nullable RecordingTabCreator mRegularRecorder;

    /**
     * @param delegate The {@link TabCreatorManager} to delegate to.
     */
    public RecordingTabCreatorManager(TabCreatorManager delegate) {
        mDelegate = delegate;
    }

    @Override
    public TabCreator getTabCreator(boolean incognito) {
        if (incognito) {
            return mDelegate.getTabCreator(/* incognito= */ true);
        }

        RecordingTabCreator recorder = getRecorder(incognito);
        if (recorder != null) {
            recorder.setDelegate(mDelegate.getTabCreator(incognito));
            return recorder;
        }

        return mDelegate.getTabCreator(incognito);
    }

    public @Nullable RecordingTabCreator getRecorder(boolean incognito) {
        assert !incognito : "Incognito is not currently supported";

        if (TabStateStorageFlagHelper.isTabStorageEnabled() && mRegularRecorder == null) {
            mRegularRecorder = new RecordingTabCreator();
        }
        return mRegularRecorder;
    }
}
