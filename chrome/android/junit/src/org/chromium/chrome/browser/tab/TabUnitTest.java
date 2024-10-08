// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
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
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tests for {@link Tab}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;

    @Rule public JniMocker mocker = new JniMocker();

    @Mock private AutofillProvider mAutofillProvider;
    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LoadUrlParams mLoadUrlParams;
    @Mock private EmptyTabObserver mObserver;
    @Mock private Context mContext;
    @Mock private WeakReference<Context> mWeakReferenceContext;
    @Mock private WeakReference<Activity> mWeakReferenceActivity;
    @Mock private Activity mActivity;
    @Mock private NativePage mNativePage;
    @Mock private TabDelegateFactory mDelegateFactory;
    @Mock private TabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;
    @Mock private WebContents mWebContents;
    @Mock private View mNativePageView;
    @Mock private ChromeActivity mChromeActivity;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private PrefService mPrefs;
    @Mock TabImpl.Natives mNativeMock;

    private TabImpl mTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(mWeakReferenceActivity).when(mWindowAndroid).getActivity();
        doReturn(mWeakReferenceContext).when(mWindowAndroid).getContext();
        doReturn(mActivity).when(mWeakReferenceActivity).get();
        doReturn(mContext).when(mWeakReferenceContext).get();
        doReturn(mContext).when(mContext).getApplicationContext();
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefs);

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
    public void testSetRootIdWithChange() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(
                TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertThat(mTab.getRootId(), equalTo(TAB1_ID));

        mTab.setRootId(TAB2_ID);

        verify(mObserver).onRootIdChanged(mTab, TAB2_ID);

        assertThat(mTab.getRootId(), equalTo(TAB2_ID));
        assertThat(
                TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.DIRTY));
    }

    @Test
    @SmallTest
    public void testSetRootIdWithoutChange() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(
                TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertThat(mTab.getRootId(), equalTo(TAB1_ID));
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setRootId(TAB1_ID);

        verify(mObserver, never()).onRootIdChanged(any(Tab.class), anyInt());
        assertThat(mTab.getRootId(), equalTo(TAB1_ID));
        assertThat(
                TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
    }

    @Test
    @SmallTest
    public void testSetTabGroupIdWithChange() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(
                TabStateAttributes.from(mTab).getDirtinessState(),
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

        TabStateAttributes attributes = TabStateAttributes.from(mTab);
        assertThat(mTab.getTabGroupId(), equalTo(token));
        assertThat(
                attributes.getDirtinessState(), equalTo(TabStateAttributes.DirtinessState.DIRTY));

        attributes.clearTabStateDirtiness();
    }

    @Test
    @SmallTest
    public void testSetTabGroupIdWithoutChange() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(
                TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertNull(mTab.getTabGroupId());
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setTabGroupId(null);

        verify(mObserver, never()).onTabGroupIdChanged(any(Tab.class), any());
        assertNull(mTab.getTabGroupId());
        assertThat(
                TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
    }

    @Test
    @SmallTest
    public void testSetTabHasSensitiveContentWithChange() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes attributes = TabStateAttributes.from(mTab);

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
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes attributes = TabStateAttributes.from(mTab);

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
    public void testFreezeDetachedNativePage() {
        mocker.mock(TabImplJni.TEST_HOOKS, mNativeMock);

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
    @DisableFeatures({AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID})
    public void testAutofillUnavailable() {
        assertFalse(mTab.providesAutofillStructure());
        mTab.setAutofillProvider(null);

        mTab.onProvideAutofillVirtualStructure(mock(ViewStructure.class), 0);
        verify(mAutofillProvider, never()).onProvideAutoFillVirtualStructure(any(), anyInt());

        mTab.autofill(new SparseArray<AutofillValue>());
        verify(mAutofillProvider, never()).autofill(any());
    }

    @Test
    @SmallTest
    @EnableFeatures({AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID})
    public void testAutofillUnavailableWithoutPref() {
        when(mPrefs.getBoolean(TabImpl.AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE)).thenReturn(false);
        assertFalse(mTab.providesAutofillStructure());
        mTab.setAutofillProvider(null);

        mTab.onProvideAutofillVirtualStructure(mock(ViewStructure.class), 0);
        verify(mAutofillProvider, never()).onProvideAutoFillVirtualStructure(any(), anyInt());

        mTab.autofill(new SparseArray<AutofillValue>());
        verify(mAutofillProvider, never()).autofill(any());
    }

    @Test
    @SmallTest
    @EnableFeatures({AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID})
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

        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        mTab.autofill(values);
        verify(mAutofillProvider).autofill(values);
    }

    @Test
    @SmallTest
    public void testDefaultInvalidTimestamp() {
        Tab tab = new TabImpl(1, mProfile, TabLaunchType.FROM_LINK);
        assertThat(tab.getTimestampMillis(), equalTo(TabImpl.INVALID_TIMESTAMP));
    }
}
