// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsInTab;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tabmodel.SettableLookAheadObservableSupplier;
import org.chromium.chrome.browser.ui.native_page.BeforeUnloadCallback;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.autofill.AndroidAutofillFeatures;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillProviderJni;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.components.tabs.DetachReason;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;

/** Tests for {@link Tab}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private AutofillProvider mAutofillProvider;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabObserver mObserver;
    @Mock private Context mContext;
    @Mock private WeakReference<Context> mWeakReferenceContext;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;
    @Mock private Activity mActivity;
    @Mock private NativePage mNativePage;
    @Mock private TabDelegateFactory mDelegateFactory;
    @Mock private TabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;

    @Mock(extraInterfaces = {WebContentsObserver.Observable.class})
    private WebContents mWebContents;

    @Mock private NavigationController mNavigationController;

    @Mock private View mNativePageView;
    @Mock private ChromeActivity mChromeActivity;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private PrefService mPrefs;
    @Mock private AutofillProvider.Natives mAutofillProviderNatives;
    @Mock TabImpl.Natives mNativeMock;
    @Mock private SecurityStateModel.Natives mSecurityStateModelNatives;
    @Mock private SelectionPopupControllerImpl mSelectionPopupController;
    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;

    private final SettableLookAheadObservableSupplier<Tab> mTabSupplier =
            new SettableLookAheadObservableSupplier<>();
    private TabImpl mTab;

    @Before
    public void setUp() {

        doReturn(mWeakReferenceActivity).when(mWindowAndroid).getActivity();
        doReturn(mWeakReferenceContext).when(mWindowAndroid).getContext();
        doReturn(ObservableSuppliers.alwaysFalse()).when(mWindowAndroid).getOcclusionSupplier();
        doReturn(mActivity).when(mWeakReferenceActivity).get();
        doReturn(mContext).when(mWeakReferenceContext).get();
        doReturn(mContext).when(mContext).getApplicationContext();
        UserPrefsJni.setInstanceForTesting(mUserPrefsNatives);
        SecurityStateModelJni.setInstanceForTesting(mSecurityStateModelNatives);
        AutofillProviderJni.setInstanceForTesting(mAutofillProviderNatives);
        WebsitePreferenceBridgeJni.setInstanceForTesting(mWebsitePreferenceBridgeJniMock);
        TabImplJni.setInstanceForTesting(mNativeMock);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefs);
        when(mWebContents.getOrSetUserData(eq(SelectionPopupControllerImpl.class), any()))
                .thenReturn(mSelectionPopupController);

        mTab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }
                };
        mTab.addObserver(mObserver);
        mTab.setAutofillProvider(mAutofillProvider);
    }

    @Test
    @SmallTest
    public void testOnAddedToTabModel_SendsDidInsertUpdate() {
        TabImplJni.setInstanceForTesting(mNativeMock);
        mTab.setNativePtrForTesting(1);

        mTab.onAddedToTabModel(mTabSupplier, ignored -> false);
        verify(mNativeMock).sendDidInsertUpdate(anyLong());
    }

    @Test
    @SmallTest
    public void testSendsDidActivateUpdate() {
        TabImplJni.setInstanceForTesting(mNativeMock);
        mTab.setNativePtrForTesting(1);

        mTab.onAddedToTabModel(mTabSupplier, ignored -> false);
        mTabSupplier.set(mTab);
        verify(mNativeMock).sendDidActivateUpdate(anyLong());
    }

    @Test
    @SmallTest
    public void testSendsWillDeactivateUpdate() {
        TabImplJni.setInstanceForTesting(mNativeMock);
        mTab.setNativePtrForTesting(1);

        mTab.onAddedToTabModel(mTabSupplier, ignored -> false);

        // Set as active first to set mWasLastActive to true.
        mTabSupplier.set(mTab);

        mTabSupplier.set(null);
        verify(mNativeMock).sendWillDeactivateUpdate(anyLong());
    }

    @Test
    @SmallTest
    public void testSetRootIdWithChange() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(
                TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class)
                        .getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertThat(mTab.getRootId(), equalTo(TAB1_ID));

        mTab.setRootId(TAB2_ID);

        verify(mObserver).onRootIdChanged(mTab, TAB2_ID);

        assertThat(mTab.getRootId(), equalTo(TAB2_ID));
        assertThat(
                TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class)
                        .getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.DIRTY));
    }

    @Test
    @SmallTest
    public void testSetRootIdWithoutChange() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(
                TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class)
                        .getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertThat(mTab.getRootId(), equalTo(TAB1_ID));
        TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class)
                .clearTabStateDirtiness();

        mTab.setRootId(TAB1_ID);

        verify(mObserver, never()).onRootIdChanged(any(Tab.class), anyInt());
        assertThat(mTab.getRootId(), equalTo(TAB1_ID));
        assertThat(
                TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class)
                        .getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
    }

    @Test
    @SmallTest
    public void testSetTabGroupIdWithChange() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(
                TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class)
                        .getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertNull(mTab.getTabGroupId());

        long tokenHigh = 0x1234567890L;
        long tokenLow = 0xABCDEF;
        Token token = new Token(tokenHigh, tokenLow);
        checkTabGroupIdChange(token);

        // Reverse field order so the token is unequal.
        token = new Token(tokenLow, tokenHigh);
        checkTabGroupIdChange(token);

        checkTabGroupIdChange(null);
    }

    private void checkTabGroupIdChange(@Nullable Token token) {
        mTab.setTabGroupId(token);

        verify(mObserver).onTabGroupIdChanged(mTab, token);

        TabStateAttributes attributes =
                TabStateAttributesRegistry.getAttributesFor(
                        mTab, TabStateAttributes.StoreKey.class);
        assertThat(mTab.getTabGroupId(), equalTo(token));
        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.DIRTY));

        attributes.clearTabStateDirtiness();
    }

    @Test
    @SmallTest
    public void testSetTabGroupIdWithoutChange() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(
                TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class)
                        .getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertNull(mTab.getTabGroupId());
        TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class)
                .clearTabStateDirtiness();

        mTab.setTabGroupId(null);

        verify(mObserver, never()).onTabGroupIdChanged(any(Tab.class), any());
        assertNull(mTab.getTabGroupId());
        assertThat(
                TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class)
                        .getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
    }

    @Test
    @SmallTest
    public void testSetTabHasSensitiveContentWithChange() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes attributes =
                TabStateAttributesRegistry.getAttributesFor(
                        mTab, TabStateAttributes.StoreKey.class);

        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertFalse(mTab.getTabHasSensitiveContent());

        mTab.setTabHasSensitiveContent(true);
        verify(mObserver).onTabContentSensitivityChanged(mTab, true);
        assertTrue(mTab.getTabHasSensitiveContent());
        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.UNTIDY));
    }

    @Test
    @SmallTest
    public void testSetTabHasSensitiveContentWithoutChange() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes attributes =
                TabStateAttributesRegistry.getAttributesFor(
                        mTab, TabStateAttributes.StoreKey.class);

        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertFalse(mTab.getTabHasSensitiveContent());

        mTab.setTabHasSensitiveContent(false);

        verify(mObserver, never()).onTabContentSensitivityChanged(any(Tab.class), anyBoolean());
        assertFalse(mTab.getTabHasSensitiveContent());
        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.CLEAN));
    }

    @Test
    @SmallTest
    public void testSetIsPinnedWithChange() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes attributes =
                TabStateAttributesRegistry.getAttributesFor(
                        mTab, TabStateAttributes.StoreKey.class);

        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertFalse(mTab.getIsPinned());

        mTab.setIsPinned(true);
        verify(mObserver).onTabPinnedStateChanged(mTab, true);
        assertTrue(mTab.getIsPinned());
        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.DIRTY));
    }

    @Test
    @SmallTest
    public void testSetIsPinnedWithoutChange() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes attributes =
                TabStateAttributesRegistry.getAttributesFor(
                        mTab, TabStateAttributes.StoreKey.class);

        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertFalse(mTab.getIsPinned());

        mTab.setIsPinned(false);

        verify(mObserver, never()).onTabPinnedStateChanged(any(Tab.class), anyBoolean());
        assertFalse(mTab.getIsPinned());
        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.CLEAN));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.PDF_REUSE_FRAGMENT})
    public void testFreezeDetachedNativePage() {
        TabImplJni.setInstanceForTesting(mNativeMock);

        doReturn(mTabWebContentsDelegateAndroid)
                .when(mDelegateFactory)
                .createWebContentsDelegate(any(Tab.class));
        doReturn(mNativePage)
                .when(mDelegateFactory)
                .createNativePage(any(String.class), any(), any(Tab.class), any());
        doReturn(false).when(mNativePage).isFrozen();
        doReturn(mNativePageView).when(mNativePage).getView();
        doReturn(mWindowAndroid).when(mWebContents).getTopLevelNativeWindow();
        doReturn(mChromeActivity).when(mWeakReferenceContext).get();

        mTab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public WindowAndroid getWindowAndroid() {
                        return mWindowAndroid;
                    }

                    @Override
                    void updateWindowAndroid(WindowAndroid windowAndroid) {}

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }

                    @Override
                    public boolean isNativePage() {
                        return true;
                    }

                    @Override
                    void pushNativePageStateToNavigationEntry() {}
                };
        mTab.updateAttachment(mWindowAndroid, mDelegateFactory);

        // A valid, non-null NativeFrozenPage object should be instantiated when a Tab is
        // told to freeze its native page in a currently detached state.
        assertEquals(mTab.getNativePage(), mNativePage);
        mTab.freezeNativePage();
        assertNotEquals(mTab.getNativePage(), mNativePage);
    }

    @Test
    @SmallTest
    public void testMaybeLoadNativePage_nullOrEmptyUrl() {
        mTab.updateAttachment(mWindowAndroid, mDelegateFactory);
        assertFalse(
                mTab.maybeShowNativePage(
                        (String) null, /* forceReload= */ false, /* pdfInfo= */ null));
        assertFalse(mTab.maybeShowNativePage("", /* forceReload= */ false, /* pdfInfo= */ null));
    }

    @Test
    @SmallTest
    public void testAutofillUnavailableWithoutPref() {
        when(mPrefs.getBoolean(TabImpl.AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE)).thenReturn(false);
        assertFalse(mTab.providesAutofillStructure());
        mTab.setAutofillProvider(null);

        mTab.onProvideAutofillVirtualStructure(mock(ViewStructure.class), 0);
        verify(mAutofillProvider, never()).onProvideAutoFillVirtualStructure(any(), anyInt());

        mTab.autofill(new SparseArray<>());
        verify(mAutofillProvider, never()).autofill(any());
    }

    @Test
    @SmallTest
    public void testAutofillRequestsHandledByProvider() {
        when(mPrefs.getBoolean(TabImpl.AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE)).thenReturn(true);
        when(mProfile.isNativeInitialized()).thenReturn(true);
        assertTrue(mTab.providesAutofillStructure());

        ViewStructure structure = mock(ViewStructure.class);
        mTab.onProvideAutofillVirtualStructure(
                structure, View.AUTOFILL_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS);
        verify(mAutofillProvider)
                .onProvideAutoFillVirtualStructure(
                        structure, View.AUTOFILL_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS);

        SparseArray<AutofillValue> values = new SparseArray<>();
        mTab.autofill(values);
        verify(mAutofillProvider).autofill(values);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        AndroidAutofillFeatures.ANDROID_AUTOFILL_LAZY_FRAMEWORK_WRAPPER_NAME,
        ChromeFeatureList.ANDROID_AUTOFILL_PREF_OBSERVER
    })
    public void testColdStart_tabInitializedBeforeProfileReady_recoversAutofill() {
        TabImplJni.setInstanceForTesting(mNativeMock);

        // 1. Simulate Cold Start: Profile is not initialized or pref is false when tab initializes
        when(mProfile.isNativeInitialized()).thenReturn(false);
        when(mPrefs.getBoolean(TabImpl.AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE)).thenReturn(false);

        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_RESTORE) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }

                    @Override
                    public Context getContext() {
                        return mContext;
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }
                };
        tab.setNativePtrForTesting(1);

        // Tab initially has no autofill provider when pref is false
        tab.mAutofillProvider = null;
        assertFalse(tab.providesAutofillStructure());
        assertNull(tab.mAutofillProvider);
        verify(mNativeMock, never()).initializeAutofillIfNecessary(anyLong());

        // 2. Simulate Profile Native Initialization & Pref Transition later (e.g.
        // AutofillClientProvider finishes)
        when(mProfile.isNativeInitialized()).thenReturn(true);
        when(mPrefs.getBoolean(TabImpl.AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE)).thenReturn(true);

        // Pref change triggers reactive state update
        tab.updateAutofillProviderState();

        // 3. User interacts with Tab 1 (calls onProvideAutofillVirtualStructure)
        ViewStructure structure = mock(ViewStructure.class);
        tab.onProvideAutofillVirtualStructure(
                structure, View.AUTOFILL_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS);

        // 4. Assert that Tab 1 has recovered and instantiated an AutofillProvider!
        assertTrue(tab.providesAutofillStructure());
        assertNotNull(
                "AutofillProvider must be initialized on Tab 1 once profile/pref is ready",
                tab.mAutofillProvider);
        verify(mNativeMock).initializeAutofillIfNecessary(1L);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        AndroidAutofillFeatures.ANDROID_AUTOFILL_LAZY_FRAMEWORK_WRAPPER_NAME,
        ChromeFeatureList.ANDROID_AUTOFILL_PREF_OBSERVER
    })
    public void testAutofillPrefDynamicToggle_updatesProviderAndImportance() {
        TabImplJni.setInstanceForTesting(mNativeMock);
        when(mProfile.isNativeInitialized()).thenReturn(true);
        when(mPrefs.getBoolean(TabImpl.AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE)).thenReturn(true);

        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_RESTORE) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }

                    @Override
                    public Context getContext() {
                        return mContext;
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }
                };
        tab.setNativePtrForTesting(1);

        // Initially enabled
        tab.updateAutofillProviderState();
        assertTrue(tab.providesAutofillStructure());
        assertNotNull(tab.mAutofillProvider);

        // Toggle to disabled
        when(mPrefs.getBoolean(TabImpl.AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE)).thenReturn(false);
        tab.updateAutofillProviderState();
        assertFalse(tab.providesAutofillStructure());
        assertNull(tab.mAutofillProvider);

        // Toggle back to enabled
        when(mPrefs.getBoolean(TabImpl.AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE)).thenReturn(true);
        tab.updateAutofillProviderState();
        assertTrue(tab.providesAutofillStructure());
        assertNotNull(tab.mAutofillProvider);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ANDROID_AUTOFILL_PREF_OBSERVER)
    public void testAutofillPrefObserver_disabled_doesNotRegisterObserver() {
        when(mProfile.isNativeInitialized()).thenReturn(true);
        when(mPrefs.getBoolean(TabImpl.AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE)).thenReturn(true);

        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }
                };

        assertNull(tab.getPrefChangeRegistrarForTesting());
    }

    @Test
    @SmallTest
    public void testDefaultInvalidTimestamp() {
        Tab tab = new TabImpl(1, mProfile, TabLaunchType.FROM_LINK);
        assertThat(tab.getTimestampMillis(), equalTo(TabImpl.INVALID_TIMESTAMP));
    }

    @Test
    @SmallTest
    public void testUpdateThemeColor_themingAllowed() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(mWebContents))
                .thenReturn(ConnectionSecurityLevel.NONE);
        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }
                };
        tab.addObserver(mObserver);

        tab.updateThemeColor(Color.RED);
        verify(mObserver).onDidChangeThemeColor(tab, Color.RED);

        tab.updateThemeColor(Color.RED);
        // Not called a second time.
        verify(mObserver).onDidChangeThemeColor(tab, Color.RED);
    }

    @Test
    @SmallTest
    public void testUpdateThemeColor_themingNotAllowed() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(mWebContents))
                .thenReturn(ConnectionSecurityLevel.NONE);
        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }
                };
        tab.addObserver(mObserver);

        // Set initial theme color while theming is allowed.
        tab.updateThemeColor(Color.RED);
        verify(mObserver).onDidChangeThemeColor(tab, Color.RED);

        // Disallow theming.
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(mWebContents))
                .thenReturn(ConnectionSecurityLevel.DANGEROUS);

        tab.updateThemeColor(Color.BLUE);
        verify(mObserver).onDidChangeThemeColor(tab, TabState.UNSPECIFIED_THEME_COLOR);

        // Calling again when already unspecified should not emit.
        tab.updateThemeColor(Color.GREEN);
        verify(mObserver, times(1)).onDidChangeThemeColor(tab, TabState.UNSPECIFIED_THEME_COLOR);
    }

    @Test
    @SmallTest
    public void testDidChangeVisibleSecurityState_themingNotAllowed() {
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(mWebContents))
                .thenReturn(ConnectionSecurityLevel.NONE);
        when(mWebContents.getThemeColor()).thenReturn(Color.RED);
        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }
                };
        tab.addObserver(mObserver);

        TabWebContentsObserver tabWebContentsObserver = TabWebContentsObserver.from(tab);
        tabWebContentsObserver.initWebContents(mWebContents);

        // Set initial theme color while theming is allowed.
        tab.updateThemeColor(Color.RED);
        verify(mObserver).onDidChangeThemeColor(tab, Color.RED);

        // Disallow theming and trigger security state change.
        when(mSecurityStateModelNatives.getSecurityLevelForWebContents(mWebContents))
                .thenReturn(ConnectionSecurityLevel.DANGEROUS);
        tabWebContentsObserver.getWebContentsObserverForTesting().didChangeVisibleSecurityState();

        verify(mObserver).onDidChangeThemeColor(tab, TabState.UNSPECIFIED_THEME_COLOR);

        // Triggering again should not emit another theme color change.
        tabWebContentsObserver.getWebContentsObserverForTesting().didChangeVisibleSecurityState();
        verify(mObserver, times(1)).onDidChangeThemeColor(tab, TabState.UNSPECIFIED_THEME_COLOR);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ABORT_NAVIGATIONS_FROM_TAB_CLOSURES})
    public void testDestroy_SendsWillDetachUpdate() {
        TabImplJni.setInstanceForTesting(mNativeMock);
        mTab.setNativePtrForTesting(1);
        doAnswer(
                        invocation -> {
                            mTab.clearNativePtr();
                            return null;
                        })
                .when(mNativeMock)
                .destroy(1);

        mTab.onAddedToTabModel(mTabSupplier, ignored -> false);
        mTab.destroy();

        verify(mNativeMock).sendWillDetachUpdate(1, DetachReason.DELETE);
        verify(mNativeMock).destroy(1);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.GLIC_BACKGROUND_ACTUATION})
    public void testStopOffscreenRendering_DestroyedWindow_PassesNullToWebContents() {
        TabImplJni.setInstanceForTesting(mNativeMock);
        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }
                };
        tab.setWebContentsForTesting(mWebContents);
        tab.setNativePtrForTesting(1);
        tab.updateWindowAndroid(mWindowAndroid);

        tab.startOffscreenRendering();
        assertTrue(tab.isOffscreenRendering());

        when(mWindowAndroid.isDestroyed()).thenReturn(true);

        clearInvocations(mWebContents);
        tab.stopOffscreenRendering();
        assertFalse(tab.isOffscreenRendering());
        verify(mWebContents).setTopLevelNativeWindow(null);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.GLIC_BACKGROUND_ACTUATION})
    public void testStopOffscreenRendering_ValidWindow_PassesWindowToWebContents() {
        TabImplJni.setInstanceForTesting(mNativeMock);
        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }
                };
        tab.setWebContentsForTesting(mWebContents);
        tab.setNativePtrForTesting(1);
        tab.updateWindowAndroid(mWindowAndroid);

        tab.startOffscreenRendering();
        assertTrue(tab.isOffscreenRendering());

        when(mWindowAndroid.isDestroyed()).thenReturn(false);

        clearInvocations(mWebContents);
        tab.stopOffscreenRendering();
        assertFalse(tab.isOffscreenRendering());
        verify(mWebContents).setTopLevelNativeWindow(mWindowAndroid);
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_SETTINGS_URL, ChromeFeatureList.SETTINGS_IN_TAB})
    public void testOnUpdateUrl_IncognitoProfile_Settings_CallsStartSettings() {
        assertTrue(SettingsInTab.isEnabled());
        SettingsNavigation mockSettingsNavigation = mock(SettingsNavigation.class);
        SettingsNavigationFactory.setInstanceForTesting(mockSettingsNavigation);
        when(mProfile.isOffTheRecord()).thenReturn(true);

        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }

                    @Override
                    public void goBack() {}
                };
        tab.updateWindowAndroid(mWindowAndroid);

        GURL settingsUrl = new GURL("chrome://settings");
        handleDidFinishNavigation(tab, settingsUrl);

        verify(mockSettingsNavigation).startSettings(any());
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_SETTINGS_URL, ChromeFeatureList.SETTINGS_IN_TAB})
    public void testOnUpdateUrl_RegularProfile_Settings_DoesNotCallStartSettings() {
        assertTrue(SettingsInTab.isEnabled());
        SettingsNavigation mockSettingsNavigation = mock(SettingsNavigation.class);
        SettingsNavigationFactory.setInstanceForTesting(mockSettingsNavigation);
        when(mProfile.isOffTheRecord()).thenReturn(false);

        TabImpl tab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }
                };
        tab.updateWindowAndroid(mWindowAndroid);

        GURL settingsUrl = new GURL("chrome://settings");
        handleDidFinishNavigation(tab, settingsUrl);

        verify(mockSettingsNavigation, never()).startSettings(any());
    }

    @Test
    @SmallTest
    public void testShow_unfreezesFrozenNativePageWhenAlreadyShown() {
        TabImplJni.setInstanceForTesting(mNativeMock);
        doReturn(mActivity).when(mWeakReferenceContext).get();
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);

        NativePage frozenNativePage = mock(NativePage.class);
        when(frozenNativePage.isFrozen()).thenReturn(true);
        when(frozenNativePage.getUrl()).thenReturn("chrome://history");

        mTab =
                new TabImpl(TAB1_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    private NativePage mCurrentNativePage = frozenNativePage;

                    @Override
                    public boolean isInitialized() {
                        return true;
                    }

                    @Override
                    public boolean isHidden() {
                        return false;
                    }

                    @Override
                    public NativePage getNativePage() {
                        return mCurrentNativePage;
                    }

                    @Override
                    void showNativePage(NativePage nativePage) {
                        mCurrentNativePage = nativePage;
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }
                };
        mTab.setNativePtrForTesting(1);
        mTab.updateAttachment(mWindowAndroid, mDelegateFactory);

        NativePage liveNativePage = mock(NativePage.class);
        when(mDelegateFactory.createNativePage(
                        eq("chrome://history"),
                        /* candidatePage= */ isNull(),
                        eq(mTab),
                        /* pdfInfo= */ isNull()))
                .thenReturn(liveNativePage);

        assertFalse(mTab.isHidden());
        assertTrue(mTab.getNativePage().isFrozen());

        // Calling show() on an already-shown tab should unfreeze the frozen native page.
        mTab.show(TabSelectionType.FROM_USER);

        verify(mDelegateFactory)
                .createNativePage(
                        eq("chrome://history"),
                        /* candidatePage= */ isNull(),
                        eq(mTab),
                        /* pdfInfo= */ isNull());
        assertEquals(liveNativePage, mTab.getNativePage());
        assertFalse(mTab.getNativePage().isFrozen());
    }

    private void handleDidFinishNavigation(TabImpl tab, GURL url) {
        tab.handleDidFinishNavigation(
                url,
                /* transitionType= */ 0,
                /* isPdf= */ false,
                /* isRendererInitiated= */ false,
                /* initiatorOrigin= */ null);
    }

    @Test
    @SmallTest
    public void testLoadUrl_BeforeUnloadCallback_Cancelled() {
        mTab.setNativePtrForTesting(1);
        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onCancel.run();
                    return true;
                };
        mTab.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);

        LoadUrlParams params = new LoadUrlParams("https://www.google.com");
        mTab.loadUrl(params);

        verify(mObserver, never()).onLoadUrl(any(), any(), any());
    }

    @Test
    @SmallTest
    public void testLoadUrl_BeforeUnloadCallback_Proceeded() {
        mTab.setNativePtrForTesting(1);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        mTab.setWebContentsForTesting(mWebContents);
        mTab.updateAttachment(mWindowAndroid, mDelegateFactory);

        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onProceed.run();
                    return true;
                };
        mTab.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);

        LoadUrlParams params = new LoadUrlParams("https://www.google.com");
        mTab.loadUrl(params);

        verify(mNavigationController).loadUrl(any());
        verify(mObserver).onLoadUrl(eq(mTab), eq(params), any());
    }

    @Test
    @SmallTest
    public void testGoBack_BeforeUnloadCallback_Cancelled() {
        when(mNavigationController.canGoBack()).thenReturn(true);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        mTab.setWebContentsForTesting(mWebContents);

        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onCancel.run();
                    return true;
                };
        mTab.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);

        mTab.goBack();
        verify(mNavigationController, never()).goBack();
    }

    @Test
    @SmallTest
    public void testGoBack_BeforeUnloadCallback_Proceeded() {
        when(mNavigationController.canGoBack()).thenReturn(true);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        mTab.setWebContentsForTesting(mWebContents);

        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onProceed.run();
                    return true;
                };
        mTab.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);

        mTab.goBack();
        verify(mNavigationController).goBack();
    }

    @Test
    @SmallTest
    public void testGoBack_CannotGoBack_BeforeUnloadNotTriggered() {
        when(mNavigationController.canGoBack()).thenReturn(false);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        mTab.setWebContentsForTesting(mWebContents);

        BeforeUnloadCallback callback = mock(BeforeUnloadCallback.class);
        mTab.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);

        mTab.goBack();
        verify(callback, never()).handleBeforeUnload(any(), any());
        verify(mNavigationController, never()).goBack();
    }

    @Test
    @SmallTest
    public void testGoForward_BeforeUnloadCallback_Cancelled() {
        when(mNavigationController.canGoForward()).thenReturn(true);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        mTab.setWebContentsForTesting(mWebContents);

        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onCancel.run();
                    return true;
                };
        mTab.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);

        mTab.goForward();
        verify(mNavigationController, never()).goForward();
    }

    @Test
    @SmallTest
    public void testGoForward_BeforeUnloadCallback_Proceeded() {
        when(mNavigationController.canGoForward()).thenReturn(true);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        mTab.setWebContentsForTesting(mWebContents);

        BeforeUnloadCallback callback =
                (onProceed, onCancel) -> {
                    onProceed.run();
                    return true;
                };
        mTab.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);

        mTab.goForward();
        verify(mNavigationController).goForward();
    }

    @Test
    @SmallTest
    public void testGoForward_CannotGoForward_BeforeUnloadNotTriggered() {
        when(mNavigationController.canGoForward()).thenReturn(false);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        mTab.setWebContentsForTesting(mWebContents);

        BeforeUnloadCallback callback = mock(BeforeUnloadCallback.class);
        mTab.getUserDataHost().setUserData(BeforeUnloadCallback.class, callback);

        mTab.goForward();
        verify(callback, never()).handleBeforeUnload(any(), any());
        verify(mNavigationController, never()).goForward();
    }
}
