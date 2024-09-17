// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;

/** Unit tests for {@link EngagementSignalsHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class EngagementSignalsHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CustomTabsConnection mConnection;
    @Mock private CustomTabsSessionToken mSession;
    @Mock private EngagementSignalsCallback mCallback;
    @Mock private TabObserverRegistrar mTabObserverRegistrar;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManager;

    private EngagementSignalsHandler mEngagementSignalsHandler;
    private ObservableSupplierImpl<Boolean> mCrashUploadPermittedSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManager);
        when(mPrivacyPreferencesManager.getUsageAndCrashReportingPermittedObservableSupplier())
                .thenReturn(mCrashUploadPermittedSupplier);
        when(mPrivacyPreferencesManager.isUsageAndCrashReportingPermitted())
                .thenAnswer(inv -> mCrashUploadPermittedSupplier.get());
        mCrashUploadPermittedSupplier.set(true);
        mEngagementSignalsHandler = new EngagementSignalsHandler(mConnection, mSession);
    }

    @Test
    public void testCreatesObserver_SetCallbackFirst() {
        mEngagementSignalsHandler.setEngagementSignalsCallback(mCallback);
        assertNull(mEngagementSignalsHandler.getEngagementSignalsObserverForTesting());
        mEngagementSignalsHandler.setTabObserverRegistrar(mTabObserverRegistrar);
        assertNotNull(mEngagementSignalsHandler.getEngagementSignalsObserverForTesting());
    }

    @Test
    public void testCreatesObserver_SetCallbackLast() {
        mEngagementSignalsHandler.setTabObserverRegistrar(mTabObserverRegistrar);
        assertNull(mEngagementSignalsHandler.getEngagementSignalsObserverForTesting());
        mEngagementSignalsHandler.setEngagementSignalsCallback(mCallback);
        assertNotNull(mEngagementSignalsHandler.getEngagementSignalsObserverForTesting());
    }

    @Test
    public void testDoesNotCreateObserverIfReportingNotPermitted() {
        mCrashUploadPermittedSupplier.set(false);
        mEngagementSignalsHandler.setEngagementSignalsCallback(mCallback);
        mEngagementSignalsHandler.setTabObserverRegistrar(mTabObserverRegistrar);
        assertNull(mEngagementSignalsHandler.getEngagementSignalsObserverForTesting());
    }

    @Test
    public void testDisableReportingDestroysObserver() {
        mEngagementSignalsHandler.setEngagementSignalsCallback(mCallback);
        mEngagementSignalsHandler.setTabObserverRegistrar(mTabObserverRegistrar);
        assertTrue(mCrashUploadPermittedSupplier.hasObservers());

        mCrashUploadPermittedSupplier.set(false);
        verify(mCallback).onSessionEnded(eq(false), any(Bundle.class));
        assertNull(mEngagementSignalsHandler.getEngagementSignalsObserverForTesting());
    }

    @Test
    public void testCloseCustomTabDestroysEverything() {
        mEngagementSignalsHandler.setEngagementSignalsCallback(mCallback);
        mEngagementSignalsHandler.setTabObserverRegistrar(mTabObserverRegistrar);
        ArgumentCaptor<CustomTabTabObserver> tabObserver =
                ArgumentCaptor.forClass(CustomTabTabObserver.class);
        assertTrue(mCrashUploadPermittedSupplier.hasObservers());
        verify(mTabObserverRegistrar, times(2)).registerActivityTabObserver(tabObserver.capture());
        var observer = mEngagementSignalsHandler.getEngagementSignalsObserverForTesting();
        // Simulate closing custom tab.
        tabObserver.getValue().onAllTabsClosed();
        // Verify observers are removed.
        assertFalse(mCrashUploadPermittedSupplier.hasObservers());
        var tabObserverInHandler =
                tabObserver.getAllValues().stream()
                        .filter(o -> !o.equals(observer))
                        .findFirst()
                        .orElseThrow();
        verify(mTabObserverRegistrar).unregisterActivityTabObserver(tabObserverInHandler);
        assertNull(mEngagementSignalsHandler.getEngagementSignalsObserverForTesting());
    }

    @Test
    public void testNotifyTabWillCloseAndReopenWithSessionReuse() {
        mEngagementSignalsHandler.setEngagementSignalsCallback(mCallback);
        mEngagementSignalsHandler.setTabObserverRegistrar(mTabObserverRegistrar);
        mEngagementSignalsHandler.notifyTabWillCloseAndReopenWithSessionReuse();
        var observer = mEngagementSignalsHandler.getEngagementSignalsObserverForTesting();
        // #notifyTabWillCloseAndReopenWithSessionReuse should suspend #onSessionEnded signals.
        assertTrue(observer.getSuspendSessionEndedForTesting());
    }
}
