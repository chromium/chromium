// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Activity;
import android.graphics.Color;
import android.net.Uri;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage.SmoothTransitionDelegate;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link TabNativePageController}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("NullAway.Init")
public class TabNativePageControllerUnitTest {
    private static final int TAB_ID = 456;
    private static final String NTP_URL = "chrome-native://newtab/";
    private static final String BOOKMARKS_URL = "chrome-native://bookmarks/";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabDelegateFactory mDelegateFactory;
    @Mock private TabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;
    @Mock private TabObserver mObserver;
    @Mock private WebContents mWebContents;
    @Mock private TabImpl.Natives mTabImplJni;
    @Mock private SecurityStateModel.Natives mSecurityStateModelJni;

    private Activity mActivity;
    private TabImpl mTab;
    private TabNativePageController mController;
    private @Nullable WindowAndroid mTopLevelWindow;
    private boolean mIsDisplayingBackForwardAnimation;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getOcclusionSupplier()).thenReturn(ObservableSuppliers.alwaysFalse());
        TabImplJni.setInstanceForTesting(mTabImplJni);
        SecurityStateModelJni.setInstanceForTesting(mSecurityStateModelJni);
        when(mDelegateFactory.createWebContentsDelegate(any()))
                .thenReturn(mTabWebContentsDelegateAndroid);
        lenient().when(mWebContents.getTopLevelNativeWindow()).thenAnswer(_ -> mTopLevelWindow);
        doAnswer(
                        inv -> {
                            mTopLevelWindow = inv.getArgument(0);
                            return null;
                        })
                .when(mWebContents)
                .setTopLevelNativeWindow(any());
        lenient()
                .doAnswer(
                        _ -> {
                            mTab.clearNativePtr();
                            return null;
                        })
                .when(mTabImplJni)
                .destroy(1L);

        mTab =
                new TabImpl(TAB_ID, mProfile, TabLaunchType.FROM_CHROME_UI) {
                    @Override
                    public boolean isInitialized() {
                        return true;
                    }

                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }

                    @Override
                    public boolean isDisplayingBackForwardAnimation() {
                        return mIsDisplayingBackForwardAnimation;
                    }
                };
        mTab.setNativePtrForTesting(1L);
        mTab.updateAttachment(mWindowAndroid, mDelegateFactory);
        mTab.addObserver(mObserver);
        mController = mTab.getNativePageControllerForTesting();
    }

    private NativePage mockNativePage(String url) {
        NativePage page = mock(NativePage.class);
        String host = Uri.parse(url).getHost();
        lenient().when(page.getUrl()).thenReturn(url);
        lenient().when(page.getHost()).thenReturn(host != null ? host : "");
        lenient().when(page.getTitle()).thenReturn("Title");
        lenient().when(page.getView()).thenReturn(new View(mActivity));
        return page;
    }

    @Test
    public void testMaybeShowNativePage() {
        mTopLevelWindow = null;
        mTab.updateAttachment(/* window= */ null, /* tabDelegateFactory= */ null);
        assertFalse(mController.maybeShowNativePage(NTP_URL, false, null));
        verify(mDelegateFactory, never()).createNativePage(any(), any(), any(), any());

        mTab.updateAttachment(mWindowAndroid, mDelegateFactory);
        assertFalse(mController.maybeShowNativePage(null, false, null));
        assertFalse(mController.maybeShowNativePage("", false, null));

        NativePage liveNtp = mockNativePage(NTP_URL);
        NativePage nextNtp = mockNativePage(NTP_URL);
        mController.showNativePage(liveNtp);
        when(mDelegateFactory.createNativePage(eq(NTP_URL), any(), eq(mTab), isNull()))
                .thenReturn(nextNtp);
        assertTrue(mController.maybeShowNativePage(NTP_URL, /* forceReload= */ false, null));
        verify(mDelegateFactory).createNativePage(NTP_URL, liveNtp, mTab, null);
        verify(mObserver).onTitleUpdated(mTab);
        verify(mObserver).onFaviconUpdated(mTab, null, null);

        assertTrue(mController.maybeShowNativePage(NTP_URL, /* forceReload= */ true, null));
        verify(mDelegateFactory).createNativePage(NTP_URL, null, mTab, null);

        mController.freezeNativePage();
        assertTrue(mController.maybeShowNativePage(NTP_URL, /* forceReload= */ false, null));
        verify(mDelegateFactory, times(2)).createNativePage(NTP_URL, null, mTab, null);

        when(mDelegateFactory.createNativePage(eq(BOOKMARKS_URL), any(), eq(mTab), isNull()))
                .thenAnswer(
                        _ -> {
                            assertTrue(
                                    mController.maybeShowNativePage(
                                            BOOKMARKS_URL, /* forceReload= */ false, null));
                            return mockNativePage(BOOKMARKS_URL);
                        });
        assertTrue(mController.maybeShowNativePage(BOOKMARKS_URL, /* forceReload= */ false, null));
        verify(mDelegateFactory, times(1))
                .createNativePage(eq(BOOKMARKS_URL), any(), eq(mTab), isNull());
    }

    @Test
    public void testShowNativePage_replacesPreviousPageAndNotifiesInOrder() {
        NativePage page1 = mockNativePage(NTP_URL);
        when(page1.getBackgroundColor()).thenReturn(Color.RED);
        mController.showNativePage(page1);
        mTab.updateThemeColor(Color.RED);
        clearInvocations(mObserver);

        mController.showNativePage(page1);
        verifyNoMoreInteractions(mObserver);

        NativePage page2 = mockNativePage(BOOKMARKS_URL);
        when(page2.getBackgroundColor()).thenReturn(Color.BLUE);
        doAnswer(
                        _ -> {
                            assertEquals(page2, mController.getNativePage());
                            verify(page1, never()).destroy();
                            return null;
                        })
                .when(mObserver)
                .onBackgroundColorChanged(mTab, Color.BLUE);

        mController.showNativePage(page2);
        InOrder order = inOrder(mObserver, page1);
        order.verify(mObserver).onBackgroundColorChanged(mTab, Color.BLUE);
        order.verify(mObserver).onDidChangeThemeColor(mTab, TabState.UNSPECIFIED_THEME_COLOR);
        order.verify(mObserver).onContentChanged(mTab);
        order.verify(page1).destroy();
    }

    @Test
    public void testHideNativePage_andBackgroundColorReset() {
        NativePage page1 = mockNativePage(NTP_URL);
        when(page1.getBackgroundColor()).thenReturn(Color.RED);
        mController.showNativePage(page1);
        clearInvocations(mObserver);

        mController.hideNativePage(/* notify= */ true);
        assertNull(mController.getNativePage());
        verify(mObserver).onContentChanged(mTab);
        verify(page1).destroy();

        mTab.notifyDidFirstVisuallyNonEmptyPaint();
        verify(mObserver).onBackgroundColorChanged(mTab, 0);
        mTab.notifyDidFirstVisuallyNonEmptyPaint();
        verifyNoMoreInteractions(mObserver);

        NativePage page2 = mockNativePage(BOOKMARKS_URL);
        when(page2.getBackgroundColor()).thenReturn(Color.RED);
        mController.showNativePage(page2);
        clearInvocations(mObserver);
        mController.hideNativePage(/* notify= */ false);
        verify(mObserver, never()).onContentChanged(mTab);
        mTab.changeWebContentBackgroundColor(Color.GREEN);
        verify(mObserver).onBackgroundColorChanged(mTab, Color.GREEN);
        mTab.notifyDidFirstVisuallyNonEmptyPaint();
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    public void testSmoothTransition_andViewDetachAlphaReset() {
        Runnable onShown = mock(Runnable.class);
        assertFalse(mController.startSmoothTransition(onShown));
        assertFalse(mController.cancelSmoothTransition());

        mIsDisplayingBackForwardAnimation = true;
        NativePage page = mockNativePage(NTP_URL);
        SmoothTransitionDelegate delegate = mock(SmoothTransitionDelegate.class);
        when(page.enableSmoothTransition()).thenReturn(delegate);
        mController.showNativePage(page);
        verify(delegate).prepare();
        assertTrue(mController.startSmoothTransition(onShown));
        verify(delegate).start(onShown);
        assertFalse(mController.startSmoothTransition(onShown));

        mIsDisplayingBackForwardAnimation = false;
        NativePage page2 = mockNativePage(BOOKMARKS_URL);
        mController.showNativePage(page2);
        View view2 = assertNonNull(page2.getView());
        view2.setAlpha(0.5f);
        mController.onViewDetachedFromWindow(view2);
        assertEquals(1.0f, view2.getAlpha(), 0f);
    }

    @Test
    public void testFreezeAndUnfreezeNativePage() {
        NativePage page = mockNativePage(NTP_URL);
        FrameLayout parent = new FrameLayout(mActivity);
        View view = assertNonNull(page.getView());
        parent.addView(view);
        mController.showNativePage(page);

        mController.freezeNativePage();
        assertEquals(page, mController.getNativePage());
        assertFalse(mController.maybeUnfreezeNativePage());

        parent.removeView(view);
        mController.freezeNativePage();
        assertTrue(assertNonNull(mController.getNativePage()).isFrozen());
        verify(page).destroy();

        NativePage recreatedPage = mockNativePage(NTP_URL);
        when(mDelegateFactory.createNativePage(NTP_URL, null, mTab, null))
                .thenReturn(recreatedPage);
        assertTrue(mController.maybeUnfreezeNativePage());
        assertEquals(recreatedPage, mController.getNativePage());
    }

    @Test
    public void testPrepareForDestroyAndDestroy_teardownOrdering() {
        FrameLayout parent = new FrameLayout(mActivity);
        NativePage pageBeforeDestroy = mockNativePage(BOOKMARKS_URL);
        View viewBeforeDestroy = assertNonNull(pageBeforeDestroy.getView());
        parent.addView(viewBeforeDestroy);
        mController.showNativePage(pageBeforeDestroy);
        doAnswer(
                        _ -> {
                            assertNull(viewBeforeDestroy.getParent());
                            assertEquals(pageBeforeDestroy, mTab.getNativePage());
                            verify(pageBeforeDestroy, never()).destroy();
                            return null;
                        })
                .when(mObserver)
                .onDestroyed(mTab);

        mTab.destroy();
        verify(mObserver).onDestroyed(mTab);
        verify(pageBeforeDestroy).destroy();
        assertNull(mController.getNativePage());
        assertEquals("Title", mTab.getTitle());
    }
}
