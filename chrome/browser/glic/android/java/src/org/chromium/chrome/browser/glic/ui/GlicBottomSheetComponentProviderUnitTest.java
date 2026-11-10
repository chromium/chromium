// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.context_sharing.R.id.expanded_content_group;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.glic.R;
import org.chromium.chrome.browser.glic.settings.GlicNavigationUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link GlicBottomSheetComponentProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicBottomSheetComponentProviderUnitTest {
    private static final long CROSSFADE_ADVANCE_MS =
            GlicBottomSheetFreTransitionController.CROSSFADE_DURATION_MS + 50;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private GlicBottomSheetComponentProvider.Natives mNativesMock;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;

    private JniRawPtr<GlicBottomSheetComponentProvider.NativeGlicSidePanelCoordinatorAndroid>
            mNativeCoordinator;
    private Context mContext;

    @Before
    public void setUp() {
        GlicBottomSheetComponentProviderJni.setInstanceForTesting(mNativesMock);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        mContext = ContextUtils.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        GlicNavigationUtils.setCustomTabLauncherForTesting(() -> mCustomTabLauncher);
    }

    @After
    public void tearDown() {
        if (mNativeCoordinator != null) {
            mNativeCoordinator.release();
            mNativeCoordinator = null;
        }
    }

    @Test
    public void testNotifyCallsNativeOnlyWithLivePointer() {
        mNativeCoordinator = spy(JniRawPtr.createForTesting(100L));
        GlicBottomSheetComponentProvider provider =
                new GlicBottomSheetComponentProvider(mProfile, mNativeCoordinator);

        provider.configureNativeFre(/* showNativeFre= */ true);
        assertTrue(provider.mShowNativeFre);
        provider.notifyNativeFreAccepted();
        assertFalse(provider.mShowNativeFre);
        verify(mNativesMock).onNativeFreAccepted(mNativeCoordinator);

        provider.destroy();
        verify(mNativeCoordinator).release();

        provider.notifyNativeFreAccepted();
        verifyNoMoreInteractions(mNativesMock);
    }

    @Test
    public void testNotifyWithNullCoordinatorIsNoOp() {
        GlicBottomSheetComponentProvider provider =
                new GlicBottomSheetComponentProvider(mProfile, /* nativeCoordinator= */ null);

        provider.configureNativeFre(/* showNativeFre= */ true);
        provider.notifyNativeFreAccepted();
        assertFalse(provider.mShowNativeFre);
        provider.destroy();

        verifyNoInteractions(mNativesMock);
    }

    @Test
    public void testIsEnterpriseAccount() {
        assertFalse(GlicEnabling.isEnterpriseAccount(null));

        when(mGlicEnablingJniMock.isEnterpriseAccount(mProfile)).thenReturn(true);
        assertTrue(GlicEnabling.isEnterpriseAccount(mProfile));

        GlicEnabling.setEnabledForTesting(true);
        assertFalse(GlicEnabling.isEnterpriseAccount(mProfile));
    }

    private View createFakeContentView() {
        FrameLayout root = new FrameLayout(mContext);
        View peekContainer = new View(mContext);
        peekContainer.setId(12345);
        root.addView(peekContainer);

        FrameLayout expanded = new FrameLayout(mContext);
        expanded.setId(expanded_content_group);
        root.addView(expanded);
        return root;
    }

    @Test
    public void testConfigureNativeFre_mountsOnActiveContentAndDetachesOnDestroy() {
        GlicBottomSheetComponentProvider provider =
                new GlicBottomSheetComponentProvider(mProfile, null);

        View contentView = createFakeContentView();
        TabBottomSheetContent content =
                provider.createContent(contentView, 0.5f, 1.0f, 0, 0, 12345, () -> {});
        assertTrue(content instanceof GlicBottomSheetContent);
        GlicBottomSheetContent glicContent = (GlicBottomSheetContent) content;

        provider.configureNativeFre(/* showNativeFre= */ true);
        assertTrue(glicContent.isNativeFreShowingForTesting());

        provider.configureNativeFre(/* showNativeFre= */ false);
        assertFalse(glicContent.isNativeFreShowingForTesting());

        provider.configureNativeFre(/* showNativeFre= */ true);
        assertTrue(glicContent.isNativeFreShowingForTesting());

        glicContent.destroy();
        assertFalse(glicContent.isNativeFreShowingForTesting());

        provider.configureNativeFre(/* showNativeFre= */ true);
        assertFalse(glicContent.isNativeFreShowingForTesting());
    }

    @Test
    public void testAcceptButton_notifiesNativeAndTearsDown() {
        mNativeCoordinator = spy(JniRawPtr.createForTesting(100L));
        GlicBottomSheetComponentProvider provider =
                new GlicBottomSheetComponentProvider(mProfile, mNativeCoordinator);

        provider.configureNativeFre(/* showNativeFre= */ true);

        View contentView = createFakeContentView();
        TabBottomSheetContent content =
                provider.createContent(contentView, 0.5f, 1.0f, 0, 0, 12345, () -> {});
        GlicBottomSheetContent glicContent = (GlicBottomSheetContent) content;
        assertTrue(glicContent.isNativeFreShowingForTesting());

        View acceptButton = contentView.findViewById(R.id.glic_fre_accept_button);
        assertNotNull(acceptButton);
        doAnswer(
                        invocation -> {
                            assertTrue(glicContent.isNativeFreShowingForTesting());
                            return null;
                        })
                .when(mNativesMock)
                .onNativeFreAccepted(mNativeCoordinator);
        acceptButton.performClick();

        verify(mNativesMock).onNativeFreAccepted(mNativeCoordinator);
        assertFalse(provider.mShowNativeFre);
        assertTrue(glicContent.isNativeFreShowingForTesting());

        ShadowLooper.idleMainLooper(CROSSFADE_ADVANCE_MS, TimeUnit.MILLISECONDS);
        assertFalse(glicContent.isNativeFreShowingForTesting());
    }

    @Test
    public void testCreateContent_tearsDownPreviousActiveContentFre() {
        GlicBottomSheetComponentProvider provider =
                new GlicBottomSheetComponentProvider(mProfile, null);
        provider.configureNativeFre(/* showNativeFre= */ true);

        View contentView = createFakeContentView();
        GlicBottomSheetContent first =
                (GlicBottomSheetContent)
                        provider.createContent(contentView, 0.5f, 1.0f, 0, 0, 12345, () -> {});
        assertTrue(first.isNativeFreShowingForTesting());

        GlicBottomSheetContent second =
                (GlicBottomSheetContent)
                        provider.createContent(contentView, 0.5f, 1.0f, 0, 0, 12345, () -> {});
        assertFalse(first.isNativeFreShowingForTesting());
        assertTrue(second.isNativeFreShowingForTesting());

        ViewGroup expanded = contentView.findViewById(expanded_content_group);
        assertEquals(1, expanded.getChildCount());

        second.destroy();
        provider.destroy();
    }
}
