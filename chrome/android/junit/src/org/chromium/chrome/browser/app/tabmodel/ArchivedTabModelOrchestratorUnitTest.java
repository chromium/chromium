// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;

/** Unit tests for {@link ArchivedTabModelOrchestrator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ArchivedTabModelOrchestratorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabPersistentStore mMockTabPersistentStore;
    @Mock private TabPersistentStore mMockShadowTabPersistentStore;
    @Mock private TabModelSelectorBase mMockTabModelSelector;
    @Mock private TabPersistencePolicy mMockTabPersistencePolicy;

    @Mock private Profile mMockProfile;

    private ArchivedTabModelOrchestrator mOrchestrator;

    @Before
    public void setUp() {
        mOrchestrator = new ArchivedTabModelOrchestrator(mMockProfile);
        mOrchestrator.initForTesting(
                mMockTabModelSelector,
                mMockTabPersistentStore,
                mMockTabPersistencePolicy,
                mMockShadowTabPersistentStore);
    }

    @Test
    public void testLoadStatePassesIgnoreRegularFiles() {
        mOrchestrator.loadState(
                /* ignoreIncognitoFiles= */ true,
                /* ignoreRegularFiles= */ true,
                /* onStandardActiveIndexRead= */ null);

        verify(mMockTabPersistentStore).loadState(eq(true), eq(true));
    }

    @Test
    public void testLoadStatePassesNoIgnoreRegularFiles() {
        mOrchestrator.loadState(
                /* ignoreIncognitoFiles= */ true,
                /* ignoreRegularFiles= */ false,
                /* onStandardActiveIndexRead= */ null);

        verify(mMockTabPersistentStore).loadState(eq(true), eq(false));
    }
}
